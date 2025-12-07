// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_session_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_handler.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
using InitiateSpotlightSessionCallback =
    base::OnceCallback<void(const std::string&)>;

constexpr char kDeviceId[] = "device-id";
constexpr char kGaiaId[] = "123";
constexpr char kRobotEmail[] = "robot@gmail.com";
constexpr char kSessionId[] = "session-id";
constexpr char kSpotlightConnectionCode[] = "456";
constexpr char kUserEmail[] = "cat@gmail.com";
constexpr char kUserFullName[] = "Best Teacher";
constexpr char kTestBaseUrl[] = "https://test";
constexpr char kOnRegisterScreenRequestSentErrorCodeUmaPath[] =
    "Ash.Boca.Spotlight.RegisterScreen.ErrorCode";
// Length of the notification duration and one extra interval for the
// notification to start.
constexpr base::TimeDelta kTestNotificationDuration =
    kSpotlightNotificationDuration + kSpotlightNotificationCountdownInterval;

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(BocaSessionManager*, GetSessionManager, (), (override));
  MOCK_METHOD(void, AddSessionManager, (BocaSessionManager*), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(std::string, GetDeviceId, (), (override));
  MOCK_METHOD(std::string, GetSchoolToolsServerBaseUrl, (), (override));
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(
            session_client_impl,
            /*pref_service=*/nullptr,
            AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId(kGaiaId)),
            /*=is_producer*/ false) {}
  MOCK_METHOD(void,
              UpdateCurrentSession,
              (std::unique_ptr<::boca::Session>, bool),
              (override));
  MOCK_METHOD((::boca::Session*), GetCurrentSession, (), (override));
  MOCK_METHOD(void, LoadCurrentSession, (bool), (override));
  ~MockSessionManager() override = default;
};

class MockSpotlightService : public SpotlightService {
 public:
  explicit MockSpotlightService(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SpotlightService(std::move(sender)) {}
  MOCK_METHOD(void,
              RegisterScreen,
              (const std::string& connection_code,
               std::string url_base,
               RegisterScreenRequestCallback callback),
              (override));
};

class MockSpotlightCrdManager : public SpotlightCrdManager {
 public:
  MOCK_METHOD(void, OnSessionEnded, (), (override));
  MOCK_METHOD(void,
              InitiateSpotlightSession,
              (InitiateSpotlightSessionCallback callback,
               bool is_student_to_receiver,
               const std::string& requester_email),
              (override));
  MOCK_METHOD(void,
              ShowPersistentNotification,
              (const std::string& teacher_name),
              (override));
  MOCK_METHOD(void, HidePersistentNotification, (), (override));
};

class FakeSpotlightNotificationHandlerDelegate
    : public SpotlightNotificationHandler::Delegate {
 public:
  FakeSpotlightNotificationHandlerDelegate() = default;
  ~FakeSpotlightNotificationHandlerDelegate() override = default;

  // SpotlightNotificationHandler::Delegate
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification) override {}
  void ClearNotification(const std::string& id) override {}
};

class SpotlightSessionManagerTest : public testing::Test {
 public:
  SpotlightSessionManagerTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kBocaSpotlight,
         ash::features::kBocaSpotlightRobotRequester},
        /*disabled_features=*/{});

    // Set up global BocaAppClient's mock.
    boca_app_client_ = std::make_unique<NiceMock<MockBocaAppClient>>();
    EXPECT_CALL(*boca_app_client_, AddSessionManager(_)).Times(1);
    ON_CALL(*boca_app_client_, GetIdentityManager())
        .WillByDefault(Return(nullptr));
    EXPECT_CALL(*boca_app_client_, GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));

    session_manager_ =
        std::make_unique<StrictMock<MockSessionManager>>(nullptr);
    ON_CALL(*boca_app_client_, GetSessionManager())
        .WillByDefault(Return(session_manager()));

    ON_CALL(*boca_app_client_, GetSchoolToolsServerBaseUrl())
        .WillByDefault(Return(kTestBaseUrl));

    auto spotlight_crd_manager =
        std::make_unique<NiceMock<MockSpotlightCrdManager>>();
    spotlight_crd_manager_ = spotlight_crd_manager.get();
    auto spotlight_service =
        std::make_unique<StrictMock<MockSpotlightService>>(nullptr);
    spotlight_service_ = spotlight_service.get();

    spotlight_session_manager_ = std::make_unique<SpotlightSessionManager>(
        std::make_unique<SpotlightNotificationHandler>(
            std::make_unique<FakeSpotlightNotificationHandlerDelegate>()),
        std::move(spotlight_crd_manager), std::move(spotlight_service));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }
  MockSessionManager* session_manager() { return session_manager_.get(); }
  MockSpotlightService* spotlight_service() { return spotlight_service_; }
  MockSpotlightCrdManager* spotlight_crd_manager() {
    return spotlight_crd_manager_.get();
  }
  std::unique_ptr<SpotlightSessionManager> spotlight_session_manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NiceMock<MockBocaAppClient>> boca_app_client_;
  std::unique_ptr<StrictMock<MockSessionManager>> session_manager_;
  raw_ptr<NiceMock<MockSpotlightCrdManager>> spotlight_crd_manager_;
  raw_ptr<StrictMock<MockSpotlightService>> spotlight_service_;
};

TEST_F(SpotlightSessionManagerTest, OnSessionStarted) {
  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
}

TEST_F(SpotlightSessionManagerTest, OnSessionEnded) {
  EXPECT_CALL(*spotlight_crd_manager(), OnSessionEnded).Times(1);
  spotlight_session_manager_->OnSessionEnded(kSessionId);
}

TEST_F(
    SpotlightSessionManagerTest,
    InitiatesSpotlightSessionWithTeacherEmailWhenBocaSpotlightRobotRequesterDisabled) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {ash::features::kBocaSpotlight},
      /*disabled_features=*/{ash::features::kBocaSpotlightRobotRequester});
  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return an connection code.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, _, kUserEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest,
       InitiatesSpotlightSessionWithServiceAccountWhenProvided) {
  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_service_account()
      ->set_email(kRobotEmail);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return an connection code.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, _, kRobotEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest,
       InitiatesSpotlightSessionUsesTeacherEmailWhenServiceAccountIsEmpty) {
  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return an connection code.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, _, kUserEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest, DoesNotStartSpotlightWithInactiveSession) {
  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);
  EXPECT_CALL(*spotlight_crd_manager(), InitiateSpotlightSession).Times(0);

  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
}

TEST_F(SpotlightSessionManagerTest, DoesNotStartSpotlightWithNoStudentStatus) {
  std::map<std::string, ::boca::StudentStatus> activities;
  EXPECT_CALL(*spotlight_crd_manager(), InitiateSpotlightSession).Times(0);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
}

TEST_F(SpotlightSessionManagerTest, DoesNotStartSpotlightWithNoDevice) {
  ::boca::StudentStatus status;
  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  EXPECT_CALL(*spotlight_crd_manager(), InitiateSpotlightSession).Times(0);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
}

TEST_F(SpotlightSessionManagerTest, DoesNotStartSpotlightIfNotRequested) {
  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::INACTIVE);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  EXPECT_CALL(*spotlight_crd_manager(), InitiateSpotlightSession).Times(0);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
}

TEST_F(SpotlightSessionManagerTest, OnlyProcessesOneRequestAtATime) {
  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  EXPECT_CALL(*spotlight_crd_manager(), InitiateSpotlightSession)
      .WillRepeatedly(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);

  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .Times(1);
  spotlight_session_manager_->OnSessionEnded(kSessionId);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest,
       InitiatesSpotlightSessionWithStudentToReceiverEnabled) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {ash::features::kBocaSpotlight,
       ash::features::kBocaSpotlightRobotRequester,
       ash::features::kBocaRedirectStudentAudioToKiosk},
      /*disabled_features=*/{});

  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_user()
      ->set_email("test@chrome-enterprise-devices.gserviceaccount.com");
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_service_account()
      ->set_email(kRobotEmail);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return a connection code and is_student_to_receiver to be
  // true.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, true, kRobotEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest,
       InitiatesSpotlightSessionWithStudentToReceiverEnabledWrongEmail) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {ash::features::kBocaSpotlight,
       ash::features::kBocaSpotlightRobotRequester,
       ash::features::kBocaRedirectStudentAudioToKiosk},
      /*disabled_features=*/{});

  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_user()
      ->set_email("test@not-enterprise.com");
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_service_account()
      ->set_email(kRobotEmail);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return a connection code and is_student_to_receiver to be
  // false, as the email is incorrect.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, false, kRobotEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

TEST_F(SpotlightSessionManagerTest,
       InitiatesSpotlightSessionWithStudentToReceiverDisabled) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {ash::features::kBocaSpotlight,
       ash::features::kBocaSpotlightRobotRequester},
      /*disabled_features=*/{ash::features::kBocaRedirectStudentAudioToKiosk});
  base::HistogramTester histograms;

  ::boca::StudentDevice device;
  device.mutable_view_screen_config()->set_view_screen_state(
      ::boca::ViewScreenConfig::REQUESTED);
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_user()
      ->set_email("test@chrome-enterprise-devices.gserviceaccount.com");
  device.mutable_view_screen_config()
      ->mutable_view_screen_requester()
      ->mutable_service_account()
      ->set_email(kRobotEmail);
  ::boca::StudentStatus status;
  status.mutable_devices()->emplace(kDeviceId, device);

  std::map<std::string, ::boca::StudentStatus> activities;
  activities.emplace(kGaiaId, status);

  // Expect CRD to return a connection code and is_student_to_receiver to be
  // false.
  EXPECT_CALL(*spotlight_crd_manager(),
              InitiateSpotlightSession(_, false, kRobotEmail))
      .WillOnce(WithArg<0>([&](auto callback) {
        std::move(callback).Run(kSpotlightConnectionCode);
      }));
  // Expect sending the code to server.
  EXPECT_CALL(*spotlight_service(),
              RegisterScreen(kSpotlightConnectionCode, kTestBaseUrl, _))
      .WillOnce(WithArg<2>(
          [&](auto callback) { std::move(callback).Run(base::ok(true)); }));
  // Expect persistent notification to show after countdown.
  EXPECT_CALL(*spotlight_crd_manager(),
              ShowPersistentNotification(kUserFullName))
      .Times(1);
  EXPECT_CALL(*session_manager(), LoadCurrentSession(false)).Times(1);

  ::boca::UserIdentity producer;
  producer.set_email(kUserEmail);
  producer.set_full_name(kUserFullName);
  spotlight_session_manager_->OnSessionStarted(kSessionId, producer);
  spotlight_session_manager_->OnConsumerActivityUpdated(activities);
  task_environment_.FastForwardBy(kTestNotificationDuration);

  histograms.ExpectTotalCount(kOnRegisterScreenRequestSentErrorCodeUmaPath, 0);
}

}  // namespace
}  // namespace ash::boca
