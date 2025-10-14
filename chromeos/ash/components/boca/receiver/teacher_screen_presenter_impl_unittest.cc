// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/teacher_screen_presenter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/get_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/shared_crd_session_wrapper.h"
#include "chromeos/ash/components/boca/util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

using ::testing::_;
using ::testing::NiceMock;

constexpr std::string_view kTeacherDeviceId = "teacher_device_id";
constexpr std::string_view kReceiverId = "receiver_id";
constexpr std::string_view kConnectionCode = "connection_code";

class MockSharedCrdSessionWrapper : public SharedCrdSessionWrapper {
 public:
  MockSharedCrdSessionWrapper() = default;
  ~MockSharedCrdSessionWrapper() override = default;
  MOCK_METHOD(void,
              StartCrdHost,
              (const std::string&,
               base::OnceCallback<void(const std::string&)>,
               base::OnceClosure,
               base::OnceClosure),
              (override));
  MOCK_METHOD(void, TerminateSession, (), (override));
};

struct TeacherScreenPresenterStartTestCase {
  std::string test_name;
  std::string get_response;
  net::HttpStatusCode get_status_code;
  std::string start_response;
  net::HttpStatusCode start_status_code;
  bool start_success;
  bool disconnected_called;
};

class TeacherScreenPresenterImplTest
    : public testing::TestWithParam<TeacherScreenPresenterStartTestCase> {
 protected:
  void SetUp() override {
    teacher_identity_.set_email("teacher@email.com");
    teacher_identity_.set_full_name("Teacher Name");
    teacher_identity_.set_gaia_id("teacher-gaia-id");
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  GURL GetReceiverUrl(std::string_view receiver_id) {
    return GURL(boca::GetSchoolToolsUrl())
        .Resolve(base::ReplaceStringPlaceholders(
            boca::kGetKioskReceiverWithoutConnectionIdUrlTemplate,
            {std::string(receiver_id)}, nullptr));
  }

  GURL GetStartReceiverUrl(std::string_view receiver_id) {
    return GURL(boca::GetSchoolToolsUrl())
        .Resolve(base::ReplaceStringPlaceholders(
            boca::kStartKioskReceiverUrlTemplate, {std::string(receiver_id)},
            nullptr));
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  ::boca::UserIdentity teacher_identity_;
  AccountInfo account_info_;
  std::unique_ptr<NiceMock<MockSharedCrdSessionWrapper>> crd_session_wrapper_ =
      std::make_unique<NiceMock<MockSharedCrdSessionWrapper>>();
};

TEST_F(TeacherScreenPresenterImplTest, StartFailureOnGetConnectionCode) {
  base::test::TestFuture<bool> start_future;
  EXPECT_CALL(*crd_session_wrapper_, StartCrdHost)
      .WillOnce([](std::string_view,
                   base::OnceCallback<void(const std::string&)>,
                   base::OnceClosure, base::OnceClosure error_callback) {
        std::move(error_callback).Run();
      });
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetReceiverUrl(kReceiverId).spec(),
                                  R"({"robotEmail":"robot@email.com"})");
  presenter.Start(kReceiverId, teacher_identity_, start_future.GetCallback(),
                  base::DoNothing());

  EXPECT_FALSE(start_future.Get());
  EXPECT_FALSE(presenter.IsPresenting());
}

TEST_F(TeacherScreenPresenterImplTest, Stop) {
  base::test::TestFuture<bool> start_future;
  base::test::TestFuture<bool> stop_future1;
  base::test::TestFuture<bool> stop_future2;
  bool disconnected_called = false;
  EXPECT_CALL(*crd_session_wrapper_, StartCrdHost("robot@email.com", _, _, _))
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(const std::string&)> success_callback,
             base::OnceClosure, base::OnceClosure session_finished_cb) {
            std::move(success_callback).Run(std::string(kConnectionCode));
          });
  EXPECT_CALL(*crd_session_wrapper_, TerminateSession).Times(1);
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetReceiverUrl(kReceiverId).spec(),
                                  R"({"robotEmail":"robot@email.com"})");
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  R"({"connectionId":"id"})");
  presenter.Start(kReceiverId, teacher_identity_, start_future.GetCallback(),
                  base::BindLambdaForTesting([&disconnected_called]() {
                    disconnected_called = true;
                  }));
  EXPECT_TRUE(start_future.Get());

  presenter.Stop(stop_future1.GetCallback());
  EXPECT_TRUE(stop_future1.Get());
  EXPECT_FALSE(disconnected_called);
  EXPECT_FALSE(presenter.IsPresenting());

  presenter.Stop(stop_future2.GetCallback());
  EXPECT_TRUE(stop_future2.Get());
}

TEST_F(TeacherScreenPresenterImplTest, StopFailsWhenStartInProgress) {
  base::test::TestFuture<bool> stop_future;
  EXPECT_CALL(*crd_session_wrapper_, TerminateSession).Times(0);
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, teacher_identity_, base::DoNothing(),
                  base::DoNothing());
  presenter.Stop(stop_future.GetCallback());

  EXPECT_FALSE(stop_future.Get());
}

TEST_F(TeacherScreenPresenterImplTest, OverlapStartShouldFail) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, teacher_identity_, start_future1.GetCallback(),
                  base::DoNothing());
  presenter.Start(kReceiverId, teacher_identity_, start_future2.GetCallback(),
                  base::DoNothing());

  EXPECT_FALSE(start_future2.Get());
  EXPECT_FALSE(start_future1.IsReady());
}

TEST_P(TeacherScreenPresenterImplTest, Start) {
  base::test::TestFuture<bool> start_future;
  base::OnceClosure session_finished_callback = base::DoNothing();
  bool disconnected_called = false;
  ON_CALL(*crd_session_wrapper_, StartCrdHost("robot@email.com", _, _, _))
      .WillByDefault(
          [&session_finished_callback](
              std::string_view,
              base::OnceCallback<void(const std::string&)> success_callback,
              base::OnceClosure, base::OnceClosure session_finished_cb) {
            session_finished_callback = std::move(session_finished_cb);
            std::move(success_callback).Run(std::string(kConnectionCode));
          });
  EXPECT_CALL(*crd_session_wrapper_, TerminateSession).Times(0);
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetReceiverUrl(kReceiverId).spec(),
                                  GetParam().get_response,
                                  GetParam().get_status_code);
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  GetParam().start_response,
                                  GetParam().start_status_code);
  presenter.Start(kReceiverId, teacher_identity_, start_future.GetCallback(),
                  base::BindLambdaForTesting([&disconnected_called]() {
                    disconnected_called = true;
                  }));

  EXPECT_TRUE(presenter.IsPresenting());
  EXPECT_EQ(start_future.Get(), GetParam().start_success);

  std::move(session_finished_callback).Run();
  EXPECT_EQ(disconnected_called, GetParam().disconnected_called);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TeacherScreenPresenterImplTest,
    testing::ValuesIn<TeacherScreenPresenterStartTestCase>({
        {.test_name = "Success",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = true,
         .disconnected_called = true},
        {.test_name = "FailureOnGetReceiver",
         .get_response = "",
         .get_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = false,
         .disconnected_called = false},
        {.test_name = "FailureOnStartReceiver",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = "",
         .start_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_success = false,
         .disconnected_called = false},
    }),
    [](const testing::TestParamInfo<TeacherScreenPresenterImplTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace ash::boca
