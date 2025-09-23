// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/student_screen_presenter.h"

#include <optional>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

constexpr std::string_view kSessionId = "session-id";
constexpr std::string_view kReceiverId = "receiver-id";
constexpr std::string_view kTeacherDeviceId = "teacher_device_id";
constexpr std::string_view kStudentDeviceId = "student_device_id";
constexpr std::string_view kConnectionIdPair =
    R"({"connectionId": "connection-id"})";

class StudentScreenPresenterImplTest : public testing::Test {
 protected:
  void SetUp() override {
    teacher_identity_.set_email("teacher@email.com");
    teacher_identity_.set_full_name("Teacher Name");
    teacher_identity_.set_gaia_id("teacher-gaia-id");
    student_identity_.set_email("student@email.com");
    student_identity_.set_full_name("Student Name");
    student_identity_.set_gaia_id("student-gaia-id");
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  GURL GetStartReceiverUrl(std::string_view receiver_id) {
    return GURL(boca::GetSchoolToolsUrl())
        .Resolve(base::ReplaceStringPlaceholders(
            boca::kStartKioskReceiverUrlTemplate, {std::string(receiver_id)},
            nullptr));
  }

  std::optional<base::Value::Dict> GetRequestBodyAndRespond(
      GURL url,
      std::string_view response,
      net::HttpStatusCode status = net::HTTP_OK) {
    url_loader_factory_.WaitForRequest(url);
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        url_loader_factory_.GetPendingRequest(0);
    const network::ResourceRequestBody* body =
        pending_request->request.request_body.get();
    std::string request_body = std::string(
        (*body->elements())[0].As<network::DataElementBytes>().AsStringPiece());
    url_loader_factory_.SimulateResponseForPendingRequest(url.spec(), response,
                                                          status);
    return base::JSONReader::ReadDict(request_body);
  }

  void VerifyUserDeviceInfo(base::Value::Dict* user_device_dict,
                            ::boca::UserIdentity user_identity,
                            std::string_view device_id) {
    ASSERT_NE(user_device_dict, nullptr);
    EXPECT_EQ(*user_device_dict->FindStringByDottedPath("device.deviceId"),
              device_id);
    EXPECT_EQ(*user_device_dict->FindStringByDottedPath("user.email"),
              user_identity.email());
    EXPECT_EQ(*user_device_dict->FindStringByDottedPath("user.fullName"),
              user_identity.full_name());
    EXPECT_EQ(*user_device_dict->FindStringByDottedPath("user.gaiaId"),
              user_identity.gaia_id());
  }

  base::test::TaskEnvironment task_environment_;
  AccountInfo account_info_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  ::boca::UserIdentity teacher_identity_;
  ::boca::UserIdentity student_identity_;
};

TEST_F(StudentScreenPresenterImplTest, StartSuccess) {
  base::test::TestFuture<bool> start_future;
  StudentScreenPresenterImpl presenter(teacher_identity_, kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kSessionId, kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  std::optional<base::Value::Dict> request_dict = GetRequestBodyAndRespond(
      GetStartReceiverUrl(kReceiverId), kConnectionIdPair);

  EXPECT_TRUE(start_future.Get());
  ASSERT_TRUE(request_dict.has_value());
  EXPECT_EQ(*request_dict->FindString("sessionId"), kSessionId);
  VerifyUserDeviceInfo(
      request_dict->FindDictByDottedPath("connection.initiator"),
      teacher_identity_, kTeacherDeviceId);
  VerifyUserDeviceInfo(
      request_dict->FindDictByDottedPath("connection.presenter"),
      student_identity_, kStudentDeviceId);
}

TEST_F(StudentScreenPresenterImplTest, StartFailure) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  StudentScreenPresenterImpl presenter(teacher_identity_, kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kSessionId, kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  GetRequestBodyAndRespond(GetStartReceiverUrl(kReceiverId), "",
                           net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_FALSE(start_future1.Get());

  // Verify that a new request will be accepted.
  presenter.Start(kSessionId, kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  GetRequestBodyAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future2.Get());
}

TEST_F(StudentScreenPresenterImplTest, OverlappingStartWillFail) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  StudentScreenPresenterImpl presenter(teacher_identity_, kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kSessionId, kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  presenter.Start(kSessionId, kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_FALSE(start_future2.Get());

  GetRequestBodyAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future1.Get());
}

}  // namespace
}  // namespace ash::boca
