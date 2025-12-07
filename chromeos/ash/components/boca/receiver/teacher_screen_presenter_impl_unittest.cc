// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/teacher_screen_presenter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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

constexpr char kBocaPresentOwnScreenOutOfSessionResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.Result";
constexpr char kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.FailureReason";
constexpr char kBocaPresentOwnScreenInSessionResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.Result";
constexpr char kBocaPresentOwnScreenInSessionFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.FailureReason";

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
  bool is_in_session;
  std::string metrics_result_uma;
  int metrics_result_bucket;
  std::string metrics_reason_uma;
  int metrics_reason_bucket;
};

class TeacherScreenPresenterImplTest : public testing::Test {
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
  presenter.Start(kReceiverId, "receiverName", teacher_identity_, false,
                  start_future.GetCallback(),
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
  presenter.Start(kReceiverId, "receiverName", teacher_identity_, false,
                  base::DoNothing(), base::DoNothing());
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
  presenter.Start(kReceiverId, "receiverName", teacher_identity_, false,
                  start_future1.GetCallback(), base::DoNothing());
  presenter.Start(kReceiverId, "receiverName", teacher_identity_, false,
                  start_future2.GetCallback(), base::DoNothing());

  EXPECT_FALSE(start_future2.Get());
  EXPECT_FALSE(start_future1.IsReady());
}

TEST_F(TeacherScreenPresenterImplTest, DestructorDuringStart) {
  base::test::TestFuture<bool> start_future;
  auto presenter = std::make_unique<TeacherScreenPresenterImpl>(
      kTeacherDeviceId, std::move(crd_session_wrapper_),
      url_loader_factory_.GetSafeWeakWrapper(),
      identity_test_env_.identity_manager());

  presenter->Start(kReceiverId, "receiverName", teacher_identity_,
                   /*is_session_active=*/false, start_future.GetCallback(),
                   base::DoNothing());
  presenter.reset();
  EXPECT_FALSE(start_future.Get());
}

class StartParamsTeacherScreenPresenterImplTest
    : public TeacherScreenPresenterImplTest,
      public testing::WithParamInterface<TeacherScreenPresenterStartTestCase> {
};

TEST_P(StartParamsTeacherScreenPresenterImplTest, Start) {
  base::HistogramTester histogram_tester;
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
  presenter.Start(kReceiverId, "receiverName", teacher_identity_,
                  GetParam().is_in_session, start_future.GetCallback(),
                  base::BindLambdaForTesting([&disconnected_called]() {
                    disconnected_called = true;
                  }));

  EXPECT_TRUE(presenter.IsPresenting());
  EXPECT_EQ(start_future.Get(), GetParam().start_success);

  int failure_count = 1 - GetParam().start_success;
  histogram_tester.ExpectTotalCount(GetParam().metrics_reason_uma,
                                    failure_count);
  histogram_tester.ExpectBucketCount(GetParam().metrics_reason_uma,
                                     GetParam().metrics_reason_bucket,
                                     failure_count);
  histogram_tester.ExpectTotalCount(GetParam().metrics_result_uma, 1);
  histogram_tester.ExpectBucketCount(GetParam().metrics_result_uma,
                                     GetParam().metrics_result_bucket, 1);

  std::move(session_finished_callback).Run();
  EXPECT_EQ(disconnected_called, GetParam().disconnected_called);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartParamsTeacherScreenPresenterImplTest,
    testing::ValuesIn<TeacherScreenPresenterStartTestCase>({
        {.test_name = "Success",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = true,
         .disconnected_called = true,
         .is_in_session = true,
         .metrics_result_uma = kBocaPresentOwnScreenInSessionResultUmaPath,
         .metrics_result_bucket = 1,
         .metrics_reason_uma =
             kBocaPresentOwnScreenInSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 0},
        {.test_name = "SuccessOutOfSession",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = true,
         .disconnected_called = true,
         .is_in_session = false,
         .metrics_result_uma = kBocaPresentOwnScreenOutOfSessionResultUmaPath,
         .metrics_result_bucket = 1,
         .metrics_reason_uma =
             kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 0},
        {.test_name = "FailureOnGetReceiver",
         .get_response = "",
         .get_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = false,
         .disconnected_called = false,
         .is_in_session = true,
         .metrics_result_uma = kBocaPresentOwnScreenInSessionResultUmaPath,
         .metrics_result_bucket = 0,
         .metrics_reason_uma =
             kBocaPresentOwnScreenInSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 4},
        {.test_name = "FailureOnGetReceiverOutOfSession",
         .get_response = "",
         .get_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_response = R"({"connectionId":"id"})",
         .start_status_code = net::HTTP_OK,
         .start_success = false,
         .disconnected_called = false,
         .is_in_session = false,
         .metrics_result_uma = kBocaPresentOwnScreenOutOfSessionResultUmaPath,
         .metrics_result_bucket = 0,
         .metrics_reason_uma =
             kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 4},
        {.test_name = "FailureOnStartReceiver",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = "",
         .start_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_success = false,
         .disconnected_called = false,
         .is_in_session = true,
         .metrics_result_uma = kBocaPresentOwnScreenInSessionResultUmaPath,
         .metrics_result_bucket = 0,
         .metrics_reason_uma =
             kBocaPresentOwnScreenInSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 3},
        {.test_name = "FailureOnStartReceiverOutOfSession",
         .get_response = R"({"robotEmail":"robot@email.com"})",
         .get_status_code = net::HTTP_OK,
         .start_response = "",
         .start_status_code = net::HTTP_INTERNAL_SERVER_ERROR,
         .start_success = false,
         .disconnected_called = false,
         .is_in_session = false,
         .metrics_result_uma = kBocaPresentOwnScreenOutOfSessionResultUmaPath,
         .metrics_result_bucket = 0,
         .metrics_reason_uma =
             kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath,
         .metrics_reason_bucket = 3},
    }),
    [](const testing::TestParamInfo<
        StartParamsTeacherScreenPresenterImplTest::ParamType>& info) {
      return info.param.test_name;
    });

class MetricsTeacherScreenPresenterImplTest
    : public TeacherScreenPresenterImplTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsSessionActive() const { return GetParam(); }

  std::string ResultUmaPath() {
    if (IsSessionActive()) {
      return kBocaPresentOwnScreenInSessionResultUmaPath;
    }
    return kBocaPresentOwnScreenOutOfSessionResultUmaPath;
  }

  std::string FailureReasonUmaPath() {
    if (IsSessionActive()) {
      return kBocaPresentOwnScreenInSessionFailureReasonUmaPath;
    }
    return kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath;
  }
};

TEST_P(MetricsTeacherScreenPresenterImplTest, StartFailureOnGetConnectionCode) {
  base::HistogramTester histogram_tester;
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
  presenter.Start(kReceiverId, "receiverName", teacher_identity_,
                  IsSessionActive(), start_future.GetCallback(),
                  base::DoNothing());

  EXPECT_FALSE(start_future.Get());
  EXPECT_FALSE(presenter.IsPresenting());
  histogram_tester.ExpectTotalCount(FailureReasonUmaPath(), 1);
  histogram_tester.ExpectBucketCount(FailureReasonUmaPath(),
                                     /* kGetCrdConnectionCodeFailed */ 5, 1);
  histogram_tester.ExpectTotalCount(ResultUmaPath(), 1);
  histogram_tester.ExpectBucketCount(ResultUmaPath(), /* failure*/ 0, 1);
}

TEST_P(MetricsTeacherScreenPresenterImplTest, StartFailureAlreadyPresenting) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> start_future_1;
  base::test::TestFuture<bool> start_future_2;
  base::OnceClosure session_finished_callback = base::DoNothing();
  EXPECT_CALL(*crd_session_wrapper_, StartCrdHost("robot@email.com", _, _, _))
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(const std::string&)> success_callback,
             base::OnceClosure, base::OnceClosure session_finished_cb) {
            std::move(success_callback).Run(std::string(kConnectionCode));
          });
  TeacherScreenPresenterImpl presenter(kTeacherDeviceId,
                                       std::move(crd_session_wrapper_),
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetReceiverUrl(kReceiverId).spec(),
                                  R"({"robotEmail":"robot@email.com"})");
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  R"({"connectionId":"id"})");
  presenter.Start(kReceiverId, "receiverName", teacher_identity_,
                  IsSessionActive(), start_future_1.GetCallback(),
                  base::DoNothing());

  presenter.Start(kReceiverId, "receiverName", teacher_identity_,
                  IsSessionActive(), start_future_2.GetCallback(),
                  base::DoNothing());

  EXPECT_TRUE(start_future_1.Get());
  EXPECT_FALSE(start_future_2.Get());
  histogram_tester.ExpectTotalCount(FailureReasonUmaPath(), 1);
  histogram_tester.ExpectBucketCount(FailureReasonUmaPath(),
                                     /* kTeacherScreenShareActive */ 2, 1);
  // Called for each call to `Start`.
  histogram_tester.ExpectTotalCount(ResultUmaPath(), 2);
  histogram_tester.ExpectBucketCount(ResultUmaPath(), /* failure */ 0, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MetricsTeacherScreenPresenterImplTest,
                         testing::Bool());

}  // namespace
}  // namespace ash::boca
