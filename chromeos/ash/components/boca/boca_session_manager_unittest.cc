// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/student_heartbeat_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {

namespace {
class MockSessionClientImpl : public SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              GetSession,
              (std::unique_ptr<GetSessionRequest>),
              (override));
  MOCK_METHOD(void,
              UpdateStudentActivity,
              (std::unique_ptr<UpdateStudentActivitiesRequest>),
              (override));
  MOCK_METHOD(void,
              StudentHeartbeat,
              (std::unique_ptr<StudentHeartbeatRequest>),
              (override));
};

class MockObserver : public BocaSessionManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionStarted,
              (const std::string& session_id,
               const ::boca::UserIdentity& producer),
              (override));
  MOCK_METHOD(void,
              OnSessionEnded,
              (const std::string& session_id),
              (override));
  MOCK_METHOD(void,
              OnSessionMetadataUpdated,
              (const std::string& session_id),
              (override));
  MOCK_METHOD(void,
              OnBundleUpdated,
              (const ::boca::Bundle& bundle),
              (override));
  MOCK_METHOD(void,
              OnSessionCaptionConfigUpdated,
              (const std::string& group_name,
               const ::boca::CaptionsConfig& config,
               const std::string& tachyon_group_id),
              (override));
  MOCK_METHOD(void,
              OnLocalCaptionConfigUpdated,
              (const ::boca::CaptionsConfig& config),
              (override));
  MOCK_METHOD(void,
              OnSessionRosterUpdated,
              (const ::boca::Roster& roster),
              (override));
  MOCK_METHOD(
      void,
      OnConsumerActivityUpdated,
      ((const std::map<std::string, ::boca::StudentStatus>& activities)),
      (override));
  MOCK_METHOD(void, OnAppReloaded, (), (override));
  MOCK_METHOD(void, OnLocalCaptionClosed, (), (override));
};

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(std::string, GetDeviceId, (), (override));
  MOCK_METHOD(std::string, GetSchoolToolsServerBaseUrl, (), (override));
};

constexpr GaiaId::Literal kTestGaiaId("123");
constexpr char kTestUserEmail[] = "cat@gmail.com";
constexpr char kInitialSessionId[] = "0";
constexpr int kInitialSessionDurationInSecs = 600;
constexpr char kDeviceId[] = "myDevice";
constexpr char kTestDefaultUrl[] = "https://test";

::boca::Session GetInitialSession(base::Time inital_time) {
  ::boca::Session session_1;
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.set_session_id(kInitialSessionId);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      inital_time.InMillisecondsSinceUnixEpoch() / 1000);
  return session_1;
}

class BocaSessionManagerTestBase : public testing::Test {
 public:
  BocaSessionManagerTestBase() = default;
  void SetUp() override {
    // Sign in test user.
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));

    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    const std::string username_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(account_id, username_hash,
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    wifi_device_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);

    session_client_impl_ =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);

    observer_ = std::make_unique<StrictMock<MockObserver>>();

    boca_app_client_ = std::make_unique<StrictMock<MockBocaAppClient>>();

    // Expect to have registered session manager for current profile.
    EXPECT_CALL(*boca_app_client_, GetIdentityManager())
        .Times(2)
        .WillRepeatedly(Return(identity_manager()));
    EXPECT_CALL(*boca_app_client_, GetSchoolToolsServerBaseUrl())
        .WillRepeatedly(Return(kTestDefaultUrl));
    core_account_id_ = identity_manager()->PickAccountIdForAccount(
        signin::GetTestGaiaIdForEmail(kTestUserEmail), kTestUserEmail);
  }

  const base::TimeDelta kDefaultInSessionPollingInterval = base::Seconds(60);
  const base::TimeDelta kDefaultIndefinitePollingInterval = base::Seconds(60);
  const base::TimeDelta kDefaultStudentHeartbeatInterval = base::Seconds(60);

 protected:
  void ToggleOnline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  void ToggleOffline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateDisconnecting));
  }

  MockSessionClientImpl* session_client_impl() {
    return session_client_impl_.get();
  }
  MockObserver* observer() { return observer_.get(); }
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>&
  fake_user_manager() {
    return fake_user_manager_;
  }
  MockBocaAppClient* boca_app_client() { return boca_app_client_.get(); }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  CoreAccountId& core_account_id() { return core_account_id_; }
  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }
  PrefService& local_state() { return local_state_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::string wifi_device_path_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  // BocaAppClient should destruct after identity env.
  std::unique_ptr<StrictMock<MockBocaAppClient>> boca_app_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  // Owned by BocaSessionManager, destructed before it.
  std::unique_ptr<StrictMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<StrictMock<MockObserver>> observer_;
  TestingPrefServiceSimple local_state_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  CoreAccountId core_account_id_;
};

class BocaSessionManagerTest : public BocaSessionManagerTestBase {
 public:
  BocaSessionManagerTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeatures(
        {ash::features::kBoca, ash::features::kBocaStudentHeartbeat},
        /*disabled_features=*/{ash::features::kBocaCustomPolling});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    // Start with active session to trigger in-session polling.
    auto session_1 = std::make_unique<::boca::Session>(
        GetInitialSession(session_start_time_));
    EXPECT_CALL(*session_client_impl(), GetSession(_))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          // The first fetch at construction time will fail due to refresh token
          // not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        }))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          boca_session_manager_->ParseSessionResponse(/*from_polling=*/false,
                                                      std::move(session_1));
        }));

    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));

    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/true);
    boca_session_manager_->AddObserver(observer());

    EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);
    // Set initial network config.
    ToggleOffline();
    // Trigger network update activity.
    ToggleOnline();

    boca_session_manager_->ToggleAppStatus(/*is_app_opened=*/true);
  }

  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }

 protected:
  base::Time session_start_time_ = base::Time::Now();

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerTest, DoNothingIfSessionUpdateFailed) {
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::PARSE_ERROR));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionEnded) {
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     base::ok(nullptr));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(kInitialSessionId)).Times(1);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenBothSessionIsEmpty) {
  auto current_session = std::make_unique<::boca::Session>();
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval +
                                    base::Seconds(1));

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  // In session polling has stopped, start polling with indefinite interval now.
  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNotPollIfActiveSessionLoad) {
  auto current_session = std::make_unique<::boca::Session>();
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval -
                                    base::Seconds(1));
  boca_session_manager()->LoadCurrentSession(/*from_polling=*/false);
  // Should have triggered an interval for session load, but skipped due to
  // there was an active load.
  task_environment()->FastForwardBy(base::Seconds(2));
}

TEST_F(BocaSessionManagerTest, SkipPollingShouldAccountForAsyncInterval) {
  auto current_session = std::make_unique<::boca::Session>();
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  // Still poll in the next interval.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval);
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionStateChanged) {
  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_state(::boca::Session::PLANNING);
  session_2->set_session_id(kInitialSessionId);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(kInitialSessionId)).Times(1);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionStateIsTheSame) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);

  // Have updated one sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionMetadataUpdateWhenDurationChange) {
  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_state(::boca::Session::ACTIVE);
  session_2->set_session_id(kInitialSessionId);
  session_2->mutable_duration()->set_seconds(kInitialSessionDurationInSecs +
                                             60);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionMetadataUpdated(kInitialSessionId))
      .Times(1);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionMetadataUpdateWhenTeacherChange) {
  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_state(::boca::Session::ACTIVE);
  session_2->set_session_id(kInitialSessionId);
  session_2->mutable_teacher()->set_gaia_id("differentId");
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionMetadataUpdated(kInitialSessionId))
      .Times(1);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenLockModeChanged) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(false);
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenBundleContentChanged) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("youtube.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);
  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenBundleOrderChanged) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  active_bundle->mutable_content_configs()->Add()->set_url("youtube.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("youtube.com");
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);
  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenBundledContentNoChange) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config_2.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  // On emit once when flip from initial empty state.
  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenCurrentBundleEmpty) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionCaptionUpdated) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(true);
  caption_config_1->set_translations_enabled(true);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config_2;
  auto* caption_config_2 = session_config.mutable_captions_config();

  caption_config_2->set_captions_enabled(false);
  caption_config_2->set_translations_enabled(false);
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _, _))
      .Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionCaptionSame) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _, _))
      .Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionConfigNameNotMatch) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);
  (*session_1->mutable_student_group_configs())["unknown"] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _, _))
      .Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionRosterUpdated) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));

  auto* student_groups_1 =
      session_1->mutable_roster()->mutable_student_groups()->Add();
  student_groups_1->set_title(kMainStudentGroupName);
  student_groups_1->mutable_students()->Add()->set_email("dog1@email.com");

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  auto* student_groups_2 =
      session_2->mutable_roster()->mutable_student_groups()->Add();
  student_groups_2->set_title(kMainStudentGroupName);
  student_groups_2->mutable_students()->Add()->set_email("dog2@email.com");

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       NotifySessionUpdateWhenSessionRosterOrderUpdated) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  auto* student_groups_1 =
      session_1->mutable_roster()->mutable_student_groups()->Add();
  student_groups_1->set_title(kMainStudentGroupName);
  student_groups_1->mutable_students()->Add()->set_email("dog2@email.com");
  student_groups_1->mutable_students()->Add()->set_email("dog1@email.com");

  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  auto* student_groups_2 =
      session_2->mutable_roster()->mutable_student_groups()->Add();
  student_groups_2->set_title(kMainStudentGroupName);
  student_groups_2->mutable_students()->Add()->set_email("dog1@email.com");
  student_groups_2->mutable_students()->Add()->set_email("dog2@email.com");

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionRosterSame) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_)).Times(0);

  // Have updated one sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DISABLED_DoNotPollSessionWhenNoNetwork) {
  ToggleOffline();
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);

  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNotPollSessionWhenUserNotActive) {
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);

  // Sign in different user.
  auto account_id = AccountId::FromUserEmailGaiaId("another", GaiaId("user"));
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  fake_user_manager().Reset(
      std::make_unique<user_manager::FakeUserManager>(&local_state()));
  fake_user_manager()->AddGaiaUser(account_id,
                                   user_manager::UserType::kRegular);
  fake_user_manager()->UserLoggedIn(account_id, username_hash,
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifyLocalCaptionConfigWhenLocalChange) {
  EXPECT_CALL(*boca_app_client(), GetIdentityManager())
      .WillOnce(Return(identity_manager()));
  EXPECT_CALL(*observer(), OnLocalCaptionConfigUpdated(_)).Times(1);

  ::boca::CaptionsConfig config;
  BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionEvents(config);
}

TEST_F(BocaSessionManagerTest, NotifyLocalCaptionClosed) {
  EXPECT_CALL(*boca_app_client(), GetIdentityManager())
      .WillOnce(Return(identity_manager()));
  EXPECT_CALL(*observer(), OnLocalCaptionClosed()).Times(1);
  BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionClosed();
}

TEST_F(BocaSessionManagerTest, NotifyAppReloadEvent) {
  EXPECT_CALL(*boca_app_client(), GetIdentityManager())
      .WillOnce(Return(identity_manager()));
  EXPECT_CALL(*observer(), OnAppReloaded()).Times(1);

  BocaAppClient::Get()->GetSessionManager()->NotifyAppReload();
}

TEST_F(BocaSessionManagerTest, UpdateTabActivity) {
  std::u16string kTab(u"google.com");
  ::boca::Session session = GetInitialSession(session_start_time_);

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            EXPECT_EQ(kInitialSessionId, request->session_id());
            EXPECT_EQ(kTestGaiaId, request->gaia_id());
            EXPECT_EQ(kDeviceId, request->device_id());
            request->callback().Run(true);
          })));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);
  boca_session_manager()->UpdateTabActivity(kTab);
}

TEST_F(BocaSessionManagerTest, UpdateTabActivityWithDummyDeviceId) {
  std::u16string kTab(u"google.com");
  ::boca::Session session = GetInitialSession(session_start_time_);

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);

  EXPECT_CALL(*boca_app_client(), GetDeviceId()).WillOnce(Return(""));

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            EXPECT_EQ(kInitialSessionId, request->session_id());
            EXPECT_EQ(kTestGaiaId, request->gaia_id());
            EXPECT_EQ(BocaSessionManager::kDummyDeviceId, request->device_id());
            request->callback().Run(true);
          })));

  boca_session_manager()->UpdateTabActivity(kTab);
}

TEST_F(BocaSessionManagerTest, UpdateTabActivityWithInactiveSession) {
  ::boca::Session session;
  session.set_session_id(kInitialSessionId);

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_)).Times(0);

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), /*dispatch_event=*/false);
  boca_session_manager()->UpdateTabActivity(u"any");
}

TEST_F(BocaSessionManagerTest, UpdateTabActivityWithSameTabShouldSkip) {
  ::boca::Session session = GetInitialSession(session_start_time_);

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_)).Times(1);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);
  boca_session_manager()->UpdateTabActivity(u"tab");
  boca_session_manager()->UpdateTabActivity(u"tab");
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionActivityUpdated) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())["device1"] = std::move(device);
  (*session_1->mutable_student_statuses())["1"] = std::move(status);
  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status_1;
  ::boca::StudentDevice device_1;
  auto* activity_1 = device_1.mutable_activity();
  activity_1->mutable_active_tab()->set_title("youtube");
  (*status_1.mutable_devices())["device1"] = std::move(device_1);
  (*session_2->mutable_student_statuses())["1"] = std::move(status);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnConsumerActivityUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenStudentStateUpdated) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  status.set_state(::boca::StudentStatus::ACTIVE);
  (*session_1->mutable_student_statuses())["1"] = std::move(status);
  ::boca::StudentStatus status_1;
  status.set_state(::boca::StudentStatus::ADDED);
  (*session_1->mutable_student_statuses())["2"] = std::move(status_1);
  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status_2;
  status.set_state(::boca::StudentStatus::ADDED);
  (*session_2->mutable_student_statuses())["1"] = std::move(status_2);
  ::boca::StudentStatus status_3;
  status.set_state(::boca::StudentStatus::ADDED);
  (*session_2->mutable_student_statuses())["2"] = std::move(status);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnConsumerActivityUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       DoNotNotifySessionUpdateWhenSessionActivityNotChanged) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())["device1"] = std::move(device);
  (*session_1->mutable_student_statuses())["1"] = std::move(status);
  auto session_2 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status_1;
  ::boca::StudentDevice device_1;
  auto* activity_1 = device_1.mutable_activity();
  activity_1->mutable_active_tab()->set_title("google");
  (*status_1.mutable_devices())["device1"] = std::move(device_1);
  (*session_2->mutable_student_statuses())["1"] = std::move(status_1);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  // Only notify once for the initial update
  EXPECT_CALL(*observer(), OnConsumerActivityUpdated(_)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       DoNotNotifyEventsExceptSessionEndedWhenSessionEnded) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();
  caption_config_1->set_captions_enabled(true);
  caption_config_1->set_translations_enabled(true);
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);
  auto* student_groups_1 =
      session_1->mutable_roster()->mutable_student_groups()->Add();
  student_groups_1->set_title(kMainStudentGroupName);
  student_groups_1->mutable_students()->Add()->set_email("dog1@email.com");

  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())["device1"] = std::move(device);
  (*session_1->mutable_student_statuses())["1"] = std::move(status);

  auto session_2 = std::make_unique<::boca::Session>();

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  // Only notify once for the initial session flip.
  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _, _))
      .Times(1);
  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_)).Times(1);
  EXPECT_CALL(*observer(), OnConsumerActivityUpdated(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenPreviousSessionEmpty) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  status.set_state(::boca::StudentStatus::ACTIVE);
  (*session_1->mutable_student_statuses())["1"] = std::move(status);
  ::boca::StudentStatus status_1;
  status.set_state(::boca::StudentStatus::ADDED);
  (*session_1->mutable_student_statuses())["2"] = std::move(status_1);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     nullptr);
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnConsumerActivityUpdated(_)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, LoadSessionWhenRefreshTokenReady) {
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(2);
  // MakeAccountAvailable fires a fresh token ready event.
  identity_test_env().MakeAccountAvailable(kTestUserEmail);
  identity_test_env().SetRefreshTokenForAccount(core_account_id());
}

TEST_F(BocaSessionManagerTest, DoNotDispatchCaptionEventWhenAppNotOpened) {
  boca_session_manager()->ToggleAppStatus(/*is_app_opened=*/false);
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(true);
  caption_config_1->set_translations_enabled(true);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _, _))
      .Times(0);

  // Have updated 1 sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, SwitchBetweenAccountShouldTriggerSessionReload) {
  // Add a second user.
  auto account_id = AccountId::FromUserEmailGaiaId("different@email",
                                                   GaiaId("differentgaia"));
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  // When login new user with existing active user, it would trigger an user
  // switch event for the existing user.
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(1);
  fake_user_manager()->AddGaiaUser(account_id,
                                   user_manager::UserType::kRegular);
  fake_user_manager()->UserLoggedIn(account_id, username_hash,
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
  // Account_id mismatch, should not load.
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);
  fake_user_manager()->SwitchActiveUser(
      AccountId::FromUserEmail("different@email"));

  // Switch back to active user, load again.
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(1);
  fake_user_manager()->SwitchActiveUser(
      AccountId::FromUserEmail(kTestUserEmail));
}

TEST_F(BocaSessionManagerTest, DispatchTwoEventsWhenSessionTakeOver) {
  const std::string session_id_2 = "differentSessionId";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id_2);
  session_1->set_session_state(::boca::Session::ACTIVE);
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(session_id_2, _)).Times(1);
  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(1);

  // Have updated 1 sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       RecordMetricsIfPollingTriggerSessionStartAndEnd) {
  base::HistogramTester histogram_tester;
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     base::ok(nullptr));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(kInitialSessionId)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(kInitialSessionId, _)).Times(1);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
  histogram_tester.ExpectTotalCount(BocaSessionManager::kPollingResultHistName,
                                    2);
  histogram_tester.ExpectBucketCount(
      BocaSessionManager::kPollingResultHistName,
      BocaSessionManager::BocaPollingResult::kSessionEnd, 1);
  histogram_tester.ExpectBucketCount(
      BocaSessionManager::kPollingResultHistName,
      BocaSessionManager::BocaPollingResult::kSessionStart, 1);
}

TEST_F(BocaSessionManagerTest, RecordMetricsIfNoSessionUpdateFromPolling) {
  base::HistogramTester histogram_tester;
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }));

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  histogram_tester.ExpectTotalCount(BocaSessionManager::kPollingResultHistName,
                                    1);
  histogram_tester.ExpectBucketCount(
      BocaSessionManager::kPollingResultHistName,
      BocaSessionManager::BocaPollingResult::kNoUpdate, 1);
}

TEST_F(BocaSessionManagerTest, RecordMetricsIfInSessionUpdateFromPolling) {
  base::HistogramTester histogram_tester;
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(true);
  caption_config_1->set_translations_enabled(true);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated(_, _, _)).Times(1);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  histogram_tester.ExpectTotalCount(BocaSessionManager::kPollingResultHistName,
                                    1);
  histogram_tester.ExpectBucketCount(
      BocaSessionManager::kPollingResultHistName,
      BocaSessionManager::BocaPollingResult::kInSessionUpdate, 1);
}

TEST_F(BocaSessionManagerTest,
       SessionEndedLocallyWhenTimeUpIfNoNetworkConnection) {
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/true,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_BAD_GATEWAY));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);

  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       SessionEndedLocallyWithNewDurationWhenSessionDurationExtended) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  const int new_session_duration = 1200;
  session_1->mutable_duration()->set_seconds(new_session_duration);
  session_1->mutable_start_time()->set_seconds(
      session_start_time_.InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }))
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/true,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_BAD_GATEWAY));
      }));
  EXPECT_CALL(*observer(), OnSessionMetadataUpdated(_)).Times(1);

  // Have updated 1 sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);

  // Not ended on initial duration.
  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs -
                    kDefaultInSessionPollingInterval.InSeconds()) +
      base::Seconds(1));
  // Ended on extended duration.
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  task_environment()->FastForwardBy(
      base::Seconds(new_session_duration - kInitialSessionDurationInSecs) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       SessionEndedLocallyWithNewDurationWhenSessionDurationShortened) {
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  const int new_session_duration = 120;
  session_1->mutable_duration()->set_seconds(new_session_duration);
  session_1->mutable_start_time()->set_seconds(
      session_start_time_.InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }))
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/true,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_BAD_GATEWAY));
      }));

  EXPECT_CALL(*observer(), OnSessionMetadataUpdated(_)).Times(1);
  // Have updated 1 sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  // Ended on shortened duration.
  task_environment()->FastForwardBy(
      base::Seconds(new_session_duration -
                    kDefaultInSessionPollingInterval.InSeconds()) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));
  EXPECT_FALSE(
      boca_session_manager()->session_duration_timer_for_testing().IsRunning());
}

TEST_F(BocaSessionManagerTest,
       SessionEndedProperlyIfResumeNetworkAfterTimeout) {
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/true,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_BAD_GATEWAY));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);

  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));
  EXPECT_FALSE(
      boca_session_manager()->session_duration_timer_for_testing().IsRunning());

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     base::ok(nullptr));
      }));
  // Session already ended, no op.
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);

  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       SessionEndedLocallyWithNewDurationWhenSessionTakeOver) {
  const std::string session_id_2 = "differentSessionId";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id_2);
  session_1->set_session_state(::boca::Session::ACTIVE);
  const int new_session_duration = 1200;
  session_1->mutable_duration()->set_seconds(new_session_duration);
  session_1->mutable_start_time()->set_seconds(
      session_start_time_.InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }))
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/true,
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_BAD_GATEWAY));
      }));

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(session_id_2, _)).Times(1);

  // Have updated 1 sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  // Not ended on initial duration.
  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs) + base::Seconds(1));
  // Ended on extended duration.
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  task_environment()->FastForwardBy(
      base::Seconds(new_session_duration - kInitialSessionDurationInSecs) +
      base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, StudentHeartbeatNotCalledWithProducer) {
  ::boca::Session session = GetInitialSession(session_start_time_);

  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(1);
  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(0);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);

  task_environment()->FastForwardBy(kDefaultStudentHeartbeatInterval);
}

class BocaSessionManagerNoPollingTest : public BocaSessionManagerTestBase {
 public:
  BocaSessionManagerNoPollingTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitAndEnableFeatureWithParameters(
        ash::features::kBocaCustomPolling,
        {{ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.name, "0"},
         {ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.name,
          "0"}});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    EXPECT_CALL(*session_client_impl(), GetSession(_))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          // The first fetch at construction time will fail due to refresh token
          // not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        }));
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/true);
  }

 protected:
  base::Time session_start_time_ = base::Time::Now();
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerNoPollingTest, DoNotPollWhenPollingIntervalIsZero) {
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerNoPollingTest,
       ConsecutiveSessionShouldEndWhenNetworkOffline) {
  boca_session_manager_->AddObserver(observer());
  // Set up the test with no polling to simplify session mock.
  ::boca::Session session_1 = GetInitialSession(session_start_time_);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);

  boca_session_manager_->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);

  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));

  // Start another session
  ::boca::Session session_2;
  session_2.set_session_state(::boca::Session::ACTIVE);
  session_2.set_session_id(kInitialSessionId);
  session_2.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_2.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);

  boca_session_manager_->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_2), /*dispatch_event=*/true);

  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);

  // Verify second session ends properly.
  task_environment()->FastForwardBy(
      base::Seconds(kInitialSessionDurationInSecs) +
      base::Seconds(BocaSessionManager::kLocalSessionTrackerBufferInSeconds) +
      base::Seconds(1));
}

class BocaSessionManagerCustomPollingTest : public BocaSessionManagerTestBase {
 public:
  static constexpr int kOutOfSessionPollingInterval = 10;
  BocaSessionManagerCustomPollingTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitAndEnableFeatureWithParameters(
        ash::features::kBocaCustomPolling,
        {{ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.name,
          "10s"},
         {ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.name,
          "10s"}});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    EXPECT_CALL(*session_client_impl(), GetSession(_))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          // The first fetch at construction time will fail due to refresh token
          // not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        }));
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/true);
  }

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerCustomPollingTest, CustomPollingInterval) {
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(1);
  task_environment()->FastForwardBy(
      base::Seconds(kOutOfSessionPollingInterval + 1));
}

class BocaSessionManagerStudentHeartbeatTest
    : public BocaSessionManagerTestBase {
 protected:
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeaturesAndParameters(
        {
            /*enabled_features=*/{
                ash::features::kBocaStudentHeartbeat,
                {
                    {ash::features::
                         kBocaStudentHeartbeatPeriodicJobIntervalInSeconds.name,
                     "60s"},
                }},
            // Disable session polling so it does not interfere the student
            // heartbeat tests.
            {ash::features::kBocaCustomPolling,
             {{ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.name,
               "0"},
              {ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.name,
               "0"}}},
        },
        /*disabled_features=*/{});
    EXPECT_CALL(*session_client_impl(), GetSession(_)).WillOnce([&]() {
      // The first fetch at construction time will fail due to refresh token
      // not ready.
      boca_session_manager_->ParseSessionResponse(
          /*from_polling=*/false, base::unexpected<google_apis::ApiErrorCode>(
                                      google_apis::ApiErrorCode::NOT_READY));
    });
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    const auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/false);
  }

  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       NoStudentHeartbeatCalledWithoutActiveSession) {
  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(0);
  task_environment()->FastForwardBy(kDefaultStudentHeartbeatInterval);
}

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       StudentHeartbeatCalledWhenSessionIsActive) {
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(1);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(kDefaultStudentHeartbeatInterval);
}

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       StudentHeartbeatStoppedWhenSessionIsNotActive) {
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(0);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  session_1.set_session_state(::boca::Session::PAST);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(kDefaultStudentHeartbeatInterval * 1 +
                                    base::Seconds(1));
}

class BocaSessionManagerStudentHeartbeatCustomPollingTest
    : public BocaSessionManagerTestBase {
 protected:
  static constexpr int kStudentHeartbeatCustomPollingInterval = 10;
  BocaSessionManagerStudentHeartbeatCustomPollingTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {ash::features::kBocaStudentHeartbeat, {}},
            {ash::features::kBocaStudentHeartbeatCustomInterval,
             {
                 {ash::features::
                      kBocaStudentHeartbeatPeriodicJobIntervalInSeconds.name,
                  "10s"},
             }},
            // Disable session polling so it does not interfere the student
            // heartbeat tests.
            {ash::features::kBocaCustomPolling,
             {{ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.name,
               "0"},
              {ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.name,
               "0"}}},
        },
        /*disabled_features=*/{});
    EXPECT_CALL(*session_client_impl(), GetSession(_)).WillOnce([&]() {
      // The first fetch at construction time will fail due to refresh token
      // not ready.
      boca_session_manager_->ParseSessionResponse(
          /*from_polling=*/false, base::unexpected<google_apis::ApiErrorCode>(
                                      google_apis::ApiErrorCode::NOT_READY));
    });
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    const auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/false);
  }

  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerStudentHeartbeatCustomPollingTest,
       StudentHeartbeatCalledWhenSessionIsActive) {
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(1);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(
      base::Seconds(kStudentHeartbeatCustomPollingInterval));
}

class BocaSessionManagerStudentHeartbeatNoPollingTest
    : public BocaSessionManagerTestBase {
 public:
  BocaSessionManagerStudentHeartbeatNoPollingTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {ash::features::kBocaStudentHeartbeat, {}},
            {ash::features::kBocaStudentHeartbeatCustomInterval,
             {
                 {ash::features::
                      kBocaStudentHeartbeatPeriodicJobIntervalInSeconds.name,
                  "0"},
             }},
            // Disable session polling so it does not interfere the student
            // heartbeat tests.
            {ash::features::kBocaCustomPolling,
             {{ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.name,
               "0"},
              {ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.name,
               "0"}}},
        },
        /*disabled_features=*/{});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    EXPECT_CALL(*session_client_impl(), GetSession(_))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          // The first fetch at construction time will fail due to
          // refresh token not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        }));
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), account_id, /*is_producer=*/false);
  }

  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerStudentHeartbeatNoPollingTest,
       StudentHeartbeatNotCalled) {
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(0);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(kDefaultStudentHeartbeatInterval);
}

}  // namespace
}  // namespace ash::boca
