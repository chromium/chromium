// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/student_screen_presenter_impl.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
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
constexpr std::string_view kConnectionId = "connection-id";
constexpr std::string_view kConnectionIdPair =
    R"({"connectionId": "connection-id"})";
constexpr std::string_view kDisconnectedState = "DISCONNECTED";
constexpr std::string_view kErrorState = "ERROR";
constexpr std::string_view kStopRequestedState = "STOP_REQUESTED";

constexpr char kBocaPresentStudentScreenResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.Result";
constexpr char kBocaPresentStudentScreenFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.FailureReason";

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

  GURL GetKioskReceiverUrl(std::string_view receiver_id,
                           std::string_view connection_id) {
    return GURL(boca::GetSchoolToolsUrl())
        .Resolve(base::ReplaceStringPlaceholders(
            boca::kGetKioskReceiverUrlTemplate,
            {std::string(receiver_id), std::string(connection_id)}, nullptr));
  }

  GURL GetUpdateReceiverUrl(std::string_view receiver_id,
                            std::string_view connection_id) {
    return GURL(boca::GetSchoolToolsUrl())
        .Resolve(base::ReplaceStringPlaceholders(
            boca_receiver::UpdateKioskReceiverStateRequest::
                kRelativeUrlTemplate,
            {std::string(receiver_id), std::string(connection_id)}, nullptr));
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
    return base::JSONReader::ReadDict(request_body,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  }

  void WaitAndRespond(GURL url,
                      std::string_view response,
                      net::HttpStatusCode status = net::HTTP_OK) {
    url_loader_factory_.WaitForRequest(url);
    url_loader_factory_.SimulateResponseForPendingRequest(url.spec(), response,
                                                          status);
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

  std::string CheckConnectionStateJson(std::string_view connection_state) {
    return base::ReplaceStringPlaceholders(
        R"({"receiverConnectionState": "$1"})", {std::string(connection_state)},
        /*offsets=*/nullptr);
  }

  std::string UpdateConnectionStateJson(std::string_view connection_state) {
    return base::ReplaceStringPlaceholders(R"({"state": "$1"})",
                                           {std::string(connection_state)},
                                           /*offsets=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AccountInfo account_info_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  ::boca::UserIdentity teacher_identity_;
  ::boca::UserIdentity student_identity_;
};

TEST_F(StudentScreenPresenterImplTest, IsPresentingInitiallyFalse) {
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  EXPECT_FALSE(presenter.IsPresenting(/*student_id=*/std::nullopt));
}

TEST_F(StudentScreenPresenterImplTest, StartSuccess) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> start_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  std::optional<base::Value::Dict> request_dict = GetRequestBodyAndRespond(
      GetStartReceiverUrl(kReceiverId), kConnectionIdPair);

  EXPECT_TRUE(start_future.Get());
  EXPECT_TRUE(presenter.IsPresenting(/*student_id=*/std::nullopt));
  EXPECT_TRUE(presenter.IsPresenting(student_identity_.gaia_id()));
  EXPECT_FALSE(presenter.IsPresenting(/*student_id=*/"other-student-id"));
  ASSERT_TRUE(request_dict.has_value());
  EXPECT_EQ(*request_dict->FindString("sessionId"), kSessionId);
  VerifyUserDeviceInfo(
      request_dict->FindDictByDottedPath("connection.initiator"),
      teacher_identity_, kTeacherDeviceId);
  VerifyUserDeviceInfo(
      request_dict->FindDictByDottedPath("connection.presenter"),
      student_identity_, kStudentDeviceId);
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 0);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* success */ 1, 1);
}

TEST_F(StudentScreenPresenterImplTest, StartFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), "",
                 net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_FALSE(start_future1.Get());
  EXPECT_FALSE(presenter.IsPresenting(/*student_id=*/std::nullopt));
  EXPECT_FALSE(presenter.IsPresenting(student_identity_.gaia_id()));
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kStartKioskConnectionRequestFailed */ 3, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure */ 0, 1);

  // Verify that a new request will be accepted.
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future2.Get());
}

TEST_F(StudentScreenPresenterImplTest, OverlappingStartWillFail) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  ::boca::UserIdentity other_student_identity;
  other_student_identity.set_email("other@email.com");
  other_student_identity.set_full_name("Other Name");
  other_student_identity.set_gaia_id("other-gaia-id");
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  presenter.Start(kReceiverId, other_student_identity, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_FALSE(start_future2.Get());
  EXPECT_TRUE(presenter.IsPresenting(/*student_id=*/std::nullopt));
  EXPECT_TRUE(presenter.IsPresenting(student_identity_.gaia_id()));
  EXPECT_FALSE(presenter.IsPresenting(other_student_identity.gaia_id()));

  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future1.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kStudentScreenShareActive */ 1, 1);
  // Recorded for each call to `Start`.
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 2);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure */ 0, 1);
}

class StudentScreenPresenterImplDisconnectTest
    : public StudentScreenPresenterImplTest,
      public testing::WithParamInterface<std::string_view> {};

TEST_P(StudentScreenPresenterImplDisconnectTest, CheckConnection) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  base::test::TestFuture<void> disconnected_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  disconnected_future.GetCallback());
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future1.Get());

  url_loader_factory_.AddResponse(
      GetKioskReceiverUrl(kReceiverId, kConnectionId).spec(),
      CheckConnectionStateJson(/*connection_state=*/GetParam()));
  presenter.CheckConnection();
  EXPECT_TRUE(disconnected_future.Wait());

  // Verify that a new request will be accepted.
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future2.Get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StudentScreenPresenterImplDisconnectTest,
                         testing::ValuesIn({kDisconnectedState, kErrorState}));

TEST_F(StudentScreenPresenterImplTest, CheckConnectionNotDisconnected) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  base::test::TestFuture<void> disconnected_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  disconnected_future.GetCallback());
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);
  EXPECT_TRUE(start_future1.Get());

  url_loader_factory_.AddResponse(
      GetKioskReceiverUrl(kReceiverId,
                          CheckConnectionStateJson(kStopRequestedState))
          .spec(),
      CheckConnectionStateJson(kStopRequestedState));
  presenter.CheckConnection();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(disconnected_future.IsReady());

  // Verify that a new request will not be accepted.
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_FALSE(start_future2.Get());
}

TEST_F(StudentScreenPresenterImplTest, CheckConnectionWithoutStart) {
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.CheckConnection();
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StudentScreenPresenterImplTest, CheckConnectionBeforeStartResponse) {
  base::test::TestFuture<bool> start_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  presenter.CheckConnection();
  WaitAndRespond(GetStartReceiverUrl(kReceiverId), kConnectionIdPair);

  EXPECT_TRUE(start_future.Get());
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StudentScreenPresenterImplTest, StopSuccess) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  base::test::TestFuture<bool> stop_future;
  base::test::TestFuture<bool> overlap_stop_future;
  base::test::TestFuture<void> disconnected_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  disconnected_future.GetCallback());
  EXPECT_TRUE(start_future1.Get());

  presenter.Stop(stop_future.GetCallback());
  presenter.Stop(overlap_stop_future.GetCallback());
  std::optional<base::Value::Dict> update_request =
      GetRequestBodyAndRespond(GetUpdateReceiverUrl(kReceiverId, kConnectionId),
                               UpdateConnectionStateJson(kDisconnectedState));
  ASSERT_TRUE(update_request.has_value());
  EXPECT_EQ(*update_request->FindString("state"), kStopRequestedState);
  EXPECT_TRUE(stop_future.Get());
  EXPECT_TRUE(overlap_stop_future.Get());
  EXPECT_FALSE(disconnected_future.IsReady());

  // Verify that a new request will be accepted.
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_TRUE(start_future2.Get());
}

TEST_F(StudentScreenPresenterImplTest, StopFailure) {
  base::test::TestFuture<bool> start_future1;
  base::test::TestFuture<bool> start_future2;
  base::test::TestFuture<bool> stop_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future1.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_TRUE(start_future1.Get());

  url_loader_factory_.AddResponse(
      GetUpdateReceiverUrl(kReceiverId, kConnectionId).spec(), "",
      net::HTTP_INTERNAL_SERVER_ERROR);
  presenter.Stop(stop_future.GetCallback());
  EXPECT_FALSE(stop_future.Get());

  // Verify that a new request will not be accepted.
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future2.GetCallback(),
                  /*disconnected_cb=*/base::DoNothing());
  EXPECT_FALSE(start_future2.Get());
}

TEST_F(StudentScreenPresenterImplTest, CheckConnectionBeforeStopRequest) {
  base::test::TestFuture<bool> start_future;
  base::test::TestFuture<bool> stop_future;
  base::test::TestFuture<void> disconnected_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(),
                  disconnected_future.GetCallback());
  EXPECT_TRUE(start_future.Get());

  presenter.CheckConnection();
  url_loader_factory_.WaitForRequest(
      GetKioskReceiverUrl(kReceiverId, kConnectionId));
  presenter.Stop(stop_future.GetCallback());
  url_loader_factory_.AddResponse(
      GetKioskReceiverUrl(kReceiverId, kConnectionId).spec(),
      CheckConnectionStateJson(kDisconnectedState));
  // Check connection should be cancelled since stop is requested.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(stop_future.IsReady());

  url_loader_factory_.AddResponse(
      GetUpdateReceiverUrl(kReceiverId, kConnectionId).spec(),
      UpdateConnectionStateJson(kDisconnectedState));
  EXPECT_TRUE(stop_future.Get());
  EXPECT_FALSE(disconnected_future.IsReady());
}

TEST_F(StudentScreenPresenterImplTest, CheckConnectionBeforeStopResponse) {
  base::test::TestFuture<bool> start_future;
  base::test::TestFuture<bool> stop_future;
  base::test::TestFuture<void> disconnected_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(),
                  disconnected_future.GetCallback());
  EXPECT_TRUE(start_future.Get());

  url_loader_factory_.AddResponse(
      GetKioskReceiverUrl(kReceiverId, kConnectionId).spec(),
      CheckConnectionStateJson(kDisconnectedState));
  presenter.Stop(stop_future.GetCallback());
  // Check connection should be ignored since stop request is in progress.
  presenter.CheckConnection();
  EXPECT_FALSE(url_loader_factory_.IsPending(
      GetKioskReceiverUrl(kReceiverId, kConnectionId).spec()));

  url_loader_factory_.AddResponse(
      GetUpdateReceiverUrl(kReceiverId, kConnectionId).spec(),
      UpdateConnectionStateJson(kDisconnectedState));
  EXPECT_TRUE(stop_future.Get());
  EXPECT_FALSE(disconnected_future.IsReady());
}

TEST_F(StudentScreenPresenterImplTest, CheckConnectionAfterStopResponse) {
  base::test::TestFuture<bool> start_future;
  base::test::TestFuture<bool> stop_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  start_future.GetCallback(), base::DoNothing());
  EXPECT_TRUE(start_future.Get());

  url_loader_factory_.AddResponse(
      GetUpdateReceiverUrl(kReceiverId, kConnectionId).spec(),
      UpdateConnectionStateJson(kStopRequestedState));
  presenter.Stop(stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Get());

  presenter.CheckConnection();
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StudentScreenPresenterImplTest, StopWithoutStart) {
  base::test::TestFuture<bool> stop_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Stop(stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Get());
}

TEST_F(StudentScreenPresenterImplTest, StopBeforeStartReceiverResponse) {
  base::test::TestFuture<bool> stop_future;
  StudentScreenPresenterImpl presenter(kSessionId, teacher_identity_,
                                       kTeacherDeviceId,
                                       url_loader_factory_.GetSafeWeakWrapper(),
                                       identity_test_env_.identity_manager());
  presenter.Start(kReceiverId, student_identity_, kStudentDeviceId,
                  base::DoNothing(), base::DoNothing());

  presenter.Stop(stop_future.GetCallback());
  EXPECT_FALSE(stop_future.Get());
}

TEST_F(StudentScreenPresenterImplTest, DestructorDuringStart) {
  base::test::TestFuture<bool> start_future;
  auto presenter = std::make_unique<StudentScreenPresenterImpl>(
      kSessionId, teacher_identity_, kTeacherDeviceId,
      url_loader_factory_.GetSafeWeakWrapper(),
      identity_test_env_.identity_manager());
  presenter->Start(kReceiverId, student_identity_, kStudentDeviceId,
                   start_future.GetCallback(), base::DoNothing());
  presenter.reset();
  EXPECT_FALSE(start_future.Get());
}

TEST_F(StudentScreenPresenterImplTest, DestructorDuringStop) {
  base::test::TestFuture<bool> start_future;
  base::test::TestFuture<bool> stop_future;
  auto presenter = std::make_unique<StudentScreenPresenterImpl>(
      kSessionId, teacher_identity_, kTeacherDeviceId,
      url_loader_factory_.GetSafeWeakWrapper(),
      identity_test_env_.identity_manager());
  url_loader_factory_.AddResponse(GetStartReceiverUrl(kReceiverId).spec(),
                                  kConnectionIdPair);
  presenter->Start(kReceiverId, student_identity_, kStudentDeviceId,
                   start_future.GetCallback(), base::DoNothing());
  EXPECT_TRUE(start_future.Get());

  presenter->Stop(stop_future.GetCallback());
  presenter.reset();
  EXPECT_FALSE(stop_future.Get());
}

}  // namespace
}  // namespace ash::boca
