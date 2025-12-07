// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/soda_testing_utils.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/screen_presenter_factory.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/student_heartbeat_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "chromeos/ash/components/boca/session_api/upload_token_request.h"
#include "chromeos/ash/components/boca/student_screen_presenter.h"
#include "chromeos/ash/components/boca/teacher_screen_presenter.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
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
              (std::unique_ptr<GetSessionRequest>, bool),
              (override));
  MOCK_METHOD(void,
              UpdateStudentActivity,
              (std::unique_ptr<UpdateStudentActivitiesRequest>),
              (override));
  MOCK_METHOD(void,
              StudentHeartbeat,
              (std::unique_ptr<StudentHeartbeatRequest>),
              (override));
  MOCK_METHOD(void,
              UploadToken,
              (std::unique_ptr<UploadTokenRequest>),
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
              OnSodaStatusUpdate,
              (BocaSessionManager::SodaStatus status),
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
  MOCK_METHOD(void, OnSessionCaptionClosed, (bool), (override));
  MOCK_METHOD(void, OnReceiverInvalidation, (), (override));
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

constexpr char kTestUserEmail[] = "cat@test";
constexpr GaiaId::Literal kTestGaiaId("cat123");
constexpr char kTestUserEmail2[] = "dog@test";
constexpr GaiaId::Literal kTestGaiaId2("dog123");

constexpr char kInitialSessionId[] = "0";
constexpr int kInitialSessionDurationInSecs = 600;
constexpr char kDeviceId[] = "myDevice";
constexpr char kTestDefaultUrl[] = "https://test";
constexpr char kDefaultLanguage[] = "en-US";
constexpr char kBadLanguage[] = "unknown language";
constexpr char kUpdateStudentActivitiesErrorCodeUmaPath[] =
    "Ash.Boca.UpdateStudentActivities.ErrorCode";
constexpr char kStudentHeartbeatErrorCodeUmaPath[] =
    "Ash.Boca.StudentHeartbeat.ErrorCode";
constexpr char kBocaUploadTokenErrorCodeUmaPath[] =
    "Ash.Boca.UploadToken.ErrorCode";

::boca::Session GetInitialSession(base::Time inital_time) {
  ::boca::Session session_1;
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.set_session_id(kInitialSessionId);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      inital_time.InMillisecondsSinceUnixEpoch() / 1000);
  return session_1;
}

class MockScreenPresenterFactory : public ScreenPresenterFactory {
 public:
  MockScreenPresenterFactory() = default;
  ~MockScreenPresenterFactory() override = default;

  MOCK_METHOD(std::unique_ptr<StudentScreenPresenter>,
              CreateStudentScreenPresenter,
              (std::string_view, const ::boca::UserIdentity&, std::string_view),
              (override));

  MOCK_METHOD(std::unique_ptr<TeacherScreenPresenter>,
              CreateTeacherScreenPresenter,
              (std::string_view),
              (override));
};

class MockStudentScreenPresenter : public StudentScreenPresenter {
 public:
  MockStudentScreenPresenter() = default;
  ~MockStudentScreenPresenter() override = default;

  MOCK_METHOD(void,
              Start,
              (std::string_view,
               const ::boca::UserIdentity&,
               std::string_view,
               base::OnceCallback<void(bool)>,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void, CheckConnection, (), (override));

  MOCK_METHOD(void, Stop, (base::OnceCallback<void(bool)>), (override));

  MOCK_METHOD(bool,
              IsPresenting,
              (std::optional<std::string_view>),
              (override));
};

class MockTeacherScreenPresenter : public TeacherScreenPresenter {
 public:
  MockTeacherScreenPresenter() = default;
  ~MockTeacherScreenPresenter() override = default;

  MOCK_METHOD(void,
              Start,
              (std::string_view,
               std::string_view,
               ::boca::UserIdentity,
               bool,
               base::OnceCallback<void(bool)>,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void, Stop, (base::OnceCallback<void(bool)>), (override));

  MOCK_METHOD(bool, IsPresenting, (), (override));
};

class BocaSessionManagerTestBase : public testing::Test {
 public:
  BocaSessionManagerTestBase() = default;
  void SetUp() override {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    boca_util::RegisterPrefs(local_state_.registry());
    cros_settings_ = std::make_unique<ash::CrosSettings>();
    auto provider =
        std::make_unique<ash::FakeCrosSettingsProvider>(base::DoNothing());
    provider->Set(ash::kAccountsPrefShowUserNamesOnSignIn, true);
    cros_settings_->AddSettingsProvider(std::move(provider));

    // Register users
    const auto account_id1 =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    const auto account_id2 =
        AccountId::FromUserEmailGaiaId(kTestUserEmail2, kTestGaiaId2);
    user_manager::TestHelper::RegisterPersistedUser(local_state_, account_id1);
    user_manager::TestHelper::RegisterPersistedUser(local_state_, account_id2);

    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        &local_state_, cros_settings_.get());
    user_manager_->Initialize();

    // Sign in test user with user 1.
    user_manager_->UserLoggedIn(
        account_id1,
        user_manager::TestHelper::GetFakeUsernameHash(account_id1));
    wifi_device_path_1_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);

    wifi_device_path_2_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);
    session_client_impl_ =
        std::make_unique<NiceMock<MockSessionClientImpl>>(nullptr);

    observer_ = std::make_unique<NiceMock<MockObserver>>();

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

  void TearDown() override {
    user_manager_->Destroy();
    user_manager_.reset();
    cros_settings_.reset();
  }

  const base::TimeDelta kDefaultInSessionPollingInterval = base::Seconds(60);
  const base::TimeDelta kDefaultIndefinitePollingInterval = base::Seconds(60);
  const base::TimeDelta kDefaultStudentHeartbeatInterval = base::Seconds(30);

 protected:
  void ToggleManagedNetOnline() {
    // Update NetworkUIData for the same network won't trigger
    // OnActiveNetworksChanged as it's supposed to be updated runtime. So
    // configure a different network.
    std::unique_ptr<NetworkUIData> ui_data =
        NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_USER_POLICY);
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_1_, shill::kUIDataProperty,
        base::Value(ui_data->GetAsJson()));
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_1_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  void ToggleNonManagedNetOnline() {
    std::unique_ptr<NetworkUIData> ui_data =
        NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_NONE);
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_2_, shill::kUIDataProperty,
        base::Value(ui_data->GetAsJson()));
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_2_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  void ToggleManagedNetOffline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_1_, shill::kStateProperty,
        base::Value(shill::kStateDisconnecting));
  }

  MockSessionClientImpl* session_client_impl() {
    return session_client_impl_.get();
  }
  MockObserver* observer() { return observer_.get(); }
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
  PrefRegistrySimple* local_state_registry() { return local_state_.registry(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::string wifi_device_path_1_;
  std::string wifi_device_path_2_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  // BocaAppClient should destruct after identity env.
  std::unique_ptr<StrictMock<MockBocaAppClient>> boca_app_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  // Owned by BocaSessionManager, destructed before it.
  std::unique_ptr<NiceMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<NiceMock<MockObserver>> observer_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::CrosSettings> cros_settings_;
  std::unique_ptr<user_manager::UserManager> user_manager_;
  CoreAccountId core_account_id_;
};

class BocaSessionManagerTest : public BocaSessionManagerTestBase {
 public:
  BocaSessionManagerTest() = default;
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeatures(
        {ash::features::kBoca, ash::features::kBocaStudentHeartbeat,
         ash::features::kBocaScreenSharingStudent,
         ash::features::kBocaScreenSharingTeacher},
        /*disabled_features=*/{ash::features::kBocaCustomPolling});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    // Start with active session to trigger in-session polling.
    auto session_1 = std::make_unique<::boca::Session>(
        GetInitialSession(session_start_time_));
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
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
        session_client_impl(), &local_state(), account_id, is_producer_);
    boca_session_manager_->AddObserver(observer());

    EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);

    // Trigger network update activity.
    ToggleManagedNetOnline();
  }

  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }

 protected:
  session_manager::SessionManager device_session_manger_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
  base::Time session_start_time_ = base::Time::Now();
  bool is_producer_ = true;

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerTest, DoNothingIfSessionUpdateFailed) {
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval +
                                    base::Seconds(1));

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::move(current_session));
      }));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(1);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval);
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       DoesNotNotifyProducerSessionUpdateWhenSessionCaptionUpdated) {
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  // Producer captions notification is done through
  // `BocaSessionManager::NotifySessionCaptionProducerEvents`.
  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated).Times(0);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionCaptionProducerEvents) {
  const std::string kTachyonGroupId = "tachyon-group";
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  session->set_tachyon_group_id(kTachyonGroupId);

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session));
      }));
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval);

  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  captions_config.set_translations_enabled(true);
  ::boca::CaptionsConfig captions_notify;
  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated(_, _, kTachyonGroupId))
      .WillOnce([&captions_notify](const std::string&,
                                   const ::boca::CaptionsConfig& captions_param,
                                   const std::string&) {
        captions_notify = captions_param;
      });
  boca_session_manager()->NotifySessionCaptionProducerEvents(captions_config);

  EXPECT_TRUE(captions_notify.captions_enabled());
  EXPECT_TRUE(captions_notify.translations_enabled());
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  ToggleManagedNetOffline();
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(0);

  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval * 1 +
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
  base::HistogramTester histogram_tester;
  std::u16string kTab(u"google.com");
  ::boca::Session session = GetInitialSession(session_start_time_);

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            EXPECT_EQ(kInitialSessionId, request->session_id());
            EXPECT_EQ(kTestGaiaId, request->gaia_id());
            EXPECT_EQ(kDeviceId, request->device_id());
            request->callback().Run(true);
          }));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);
  boca_session_manager()->UpdateTabActivity(kTab);
  histogram_tester.ExpectTotalCount(kUpdateStudentActivitiesErrorCodeUmaPath,
                                    0);
}

TEST_F(BocaSessionManagerTest, UpdateTabActivityFailed) {
  base::HistogramTester histogram_tester;
  std::u16string kTab(u"google.com");
  ::boca::Session session = GetInitialSession(session_start_time_);

  EXPECT_CALL(*session_client_impl(), UpdateStudentActivity(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            request->callback().Run(base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR));
          }));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);
  boca_session_manager()->UpdateTabActivity(kTab);
  histogram_tester.ExpectTotalCount(kUpdateStudentActivitiesErrorCodeUmaPath,
                                    1);
  histogram_tester.ExpectBucketCount(
      kUpdateStudentActivitiesErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 1);
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
          [&](auto request) {
            EXPECT_EQ(kInitialSessionId, request->session_id());
            EXPECT_EQ(kTestGaiaId, request->gaia_id());
            EXPECT_EQ(BocaSessionManager::kDummyDeviceId, request->device_id());
            request->callback().Run(true);
          }));

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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

TEST_F(BocaSessionManagerTest, GetStudentActiveDeviceIdOutOfSession) {
  EXPECT_FALSE(boca_session_manager()
                   ->GetStudentActiveDeviceId("student-id")
                   .has_value());
}

TEST_F(BocaSessionManagerTest, GetStudentActiveDeviceIdNotFound) {
  base::test::TestFuture<void> signal;
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  device.set_state(::boca::StudentDevice::ACTIVE);
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())["device1"] = std::move(device);
  (*session->mutable_student_statuses())["1"] = std::move(status);
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session));
        signal.GetCallback().Run();
      }));
  boca_session_manager()->OnInvalidationReceived("payload");

  EXPECT_FALSE(boca_session_manager()
                   ->GetStudentActiveDeviceId("invalid id")
                   .has_value());
}

TEST_F(BocaSessionManagerTest, GetStudentActiveDeviceIdNoActiveDevices) {
  constexpr std::string_view kInactiveStudentId = "123";
  base::test::TestFuture<void> signal;
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  device.set_state(::boca::StudentDevice::INACTIVE);
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())["device1"] = std::move(device);
  (*session->mutable_student_statuses())[kInactiveStudentId] =
      std::move(status);
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session));
        signal.GetCallback().Run();
      }));
  boca_session_manager()->OnInvalidationReceived("payload");

  EXPECT_TRUE(signal.Wait());
  EXPECT_FALSE(boca_session_manager()
                   ->GetStudentActiveDeviceId(kInactiveStudentId)
                   .has_value());
}

TEST_F(BocaSessionManagerTest, GetStudentActiveDeviceIdFound) {
  constexpr std::string_view kActiveStudentId = "123";
  base::test::TestFuture<void> signal;
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  ::boca::StudentStatus status;
  ::boca::StudentDevice device;
  device.set_state(::boca::StudentDevice::ACTIVE);
  auto* activity = device.mutable_activity();
  activity->mutable_active_tab()->set_title("google");
  (*status.mutable_devices())[kDeviceId] = std::move(device);
  (*session->mutable_student_statuses())[kActiveStudentId] = std::move(status);
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session));
        signal.GetCallback().Run();
      }));
  boca_session_manager()->OnInvalidationReceived("payload");
  EXPECT_TRUE(signal.Wait());
  std::optional<std::string> device_id =
      boca_session_manager()->GetStudentActiveDeviceId(kActiveStudentId);
  ASSERT_TRUE(device_id.has_value());
  EXPECT_EQ(device_id.value(), kDeviceId);
}

TEST_F(BocaSessionManagerTest,
       GetScreenPresentersWithoutSettingPresenterFactory) {
  base::test::TestFuture<void> signal;
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session));
        signal.GetCallback().Run();
      }));
  boca_session_manager()->OnInvalidationReceived("payload");
  EXPECT_TRUE(signal.Wait());
  EXPECT_THAT(boca_session_manager()->GetStudentScreenPresenter(), IsNull());
  EXPECT_THAT(boca_session_manager()->GetTeacherScreenPresenter(), IsNull());
}

TEST_F(BocaSessionManagerTest, GetStudentScreenPresenter) {
  base::test::TestFuture<void> inactive_signal;
  base::test::TestFuture<void> active_signal;
  auto session =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  auto screen_presenter_factory =
      std::make_unique<MockScreenPresenterFactory>();
  EXPECT_CALL(*screen_presenter_factory, CreateStudentScreenPresenter)
      .WillOnce(Return(std::make_unique<MockStudentScreenPresenter>()));

  boca_session_manager()->SetScreenPresenterFactory(
      std::move(screen_presenter_factory));
  // Stop session.
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            /*from_polling=*/false, std::make_unique<::boca::Session>());
        inactive_signal.GetCallback().Run();
      }));
  EXPECT_CALL(*observer(), OnSessionEnded).Times(1);
  boca_session_manager()->OnInvalidationReceived("payload");
  EXPECT_TRUE(inactive_signal.Wait());
  // Start session.
  EXPECT_CALL(*session_client_impl(), GetSession).WillOnce(([&]() {
    boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                 std::move(session));
    active_signal.GetCallback().Run();
  }));
  EXPECT_CALL(*observer(), OnSessionStarted).Times(1);
  boca_session_manager()->OnInvalidationReceived("payload");
  EXPECT_TRUE(active_signal.Wait());
  EXPECT_THAT(boca_session_manager()->GetStudentScreenPresenter(), NotNull());

  boca_session_manager()->CleanupPresenters();
  EXPECT_THAT(boca_session_manager()->GetStudentScreenPresenter(), IsNull());
}

TEST_F(BocaSessionManagerTest, GetTeacherScreenPresenter) {
  auto screen_presenter_factory =
      std::make_unique<MockScreenPresenterFactory>();
  auto* const screen_presenter_factory_ptr = screen_presenter_factory.get();
  boca_session_manager()->SetScreenPresenterFactory(
      std::move(screen_presenter_factory));

  EXPECT_CALL(*screen_presenter_factory_ptr, CreateTeacherScreenPresenter)
      .WillOnce(Return(std::make_unique<MockTeacherScreenPresenter>()));
  EXPECT_THAT(boca_session_manager()->GetTeacherScreenPresenter(), NotNull());
  // A second call to `GetTeacherScreenPresenter()` will return the same teacher
  // screen presenter instance.
  EXPECT_THAT(boca_session_manager()->GetTeacherScreenPresenter(), NotNull());

  boca_session_manager()->CleanupPresenters();
  EXPECT_CALL(*screen_presenter_factory_ptr, CreateTeacherScreenPresenter)
      .WillOnce(Return(std::make_unique<MockTeacherScreenPresenter>()));
  EXPECT_THAT(boca_session_manager()->GetTeacherScreenPresenter(), NotNull());
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  // Producer notification is done through
  // `BocaSessionManager::NotifySessionCaptionProducerEvents`.
  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated).Times(0);
  // Only notify once for the initial session flip.
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(2);
  // MakeAccountAvailable fires a fresh token ready event.
  identity_test_env().MakeAccountAvailable(kTestUserEmail);
  identity_test_env().SetRefreshTokenForAccount(core_account_id());
}

TEST_F(BocaSessionManagerTest, SwitchBetweenAccountShouldTriggerSessionReload) {
  // Add a second user.
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kTestUserEmail2, kTestGaiaId2);
  const std::string username_hash =
      user_manager::TestHelper::GetFakeUsernameHash(account_id);
  // When login new user with existing active user, it would trigger an user
  // switch event for the existing user. However, it ignores the event
  // because the active user is not the one that the boca manager is
  // tracking.
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(0);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(account_id, username_hash);
  testing::Mock::VerifyAndClearExpectations(session_client_impl());

  // Account_id mismatch, should not load.
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(0);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(1);
  EXPECT_CALL(*observer(), OnSessionCaptionClosed(/*is_error=*/false)).Times(1);
  user_manager->SwitchActiveUser(account_id);
  testing::Mock::VerifyAndClearExpectations(session_client_impl());

  // Switch back to active user, load again.
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(0);
  EXPECT_CALL(*observer(), OnSessionCaptionClosed).Times(0);
  user_manager->SwitchActiveUser(
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId));
  testing::Mock::VerifyAndClearExpectations(session_client_impl());
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  histogram_tester.ExpectTotalCount(boca::kPollingResult, 2);
  histogram_tester.ExpectBucketCount(
      boca::kPollingResult, BocaSessionManager::BocaPollingResult::kSessionEnd,
      1);
  histogram_tester.ExpectBucketCount(
      boca::kPollingResult,
      BocaSessionManager::BocaPollingResult::kSessionStart, 1);
}

TEST_F(BocaSessionManagerTest, RecordMetricsIfNoSessionUpdateFromPolling) {
  base::HistogramTester histogram_tester;
  auto session_1 =
      std::make_unique<::boca::Session>(GetInitialSession(session_start_time_));
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }));

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  histogram_tester.ExpectTotalCount(boca::kPollingResult, 1);
  histogram_tester.ExpectBucketCount(
      boca::kPollingResult, BocaSessionManager::BocaPollingResult::kNoUpdate,
      1);
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/true,
                                                     std::move(session_1));
      }));

  // Producer notification is done through
  // `BocaSessionManager::NotifySessionCaptionProducerEvents`.
  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated).Times(0);
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 1 +
                                    base::Seconds(1));
  histogram_tester.ExpectTotalCount(boca::kPollingResult, 1);
  histogram_tester.ExpectBucketCount(
      boca::kPollingResult,
      BocaSessionManager::BocaPollingResult::kInSessionUpdate, 1);
}

TEST_F(BocaSessionManagerTest,
       SessionEndedLocallyWhenTimeUpIfNoNetworkConnection) {
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_)).Times(0);
  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session), false);

  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval);
}

TEST_F(BocaSessionManagerTest, InitializerSetSuccess) {
  base::test::TestFuture<bool> test_future;
  boca_session_manager()->SetSessionCaptionInitializer(
      base::BindLambdaForTesting([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(true);
      }));
  boca_session_manager()->InitSessionCaption(test_future.GetCallback());
  EXPECT_TRUE(test_future.Get());
}

TEST_F(BocaSessionManagerTest, InitializerSetFailure) {
  base::test::TestFuture<bool> test_future;
  boca_session_manager()->SetSessionCaptionInitializer(
      base::BindLambdaForTesting([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(false);
      }));
  boca_session_manager()->InitSessionCaption(test_future.GetCallback());
  EXPECT_FALSE(test_future.Get());
}

TEST_F(BocaSessionManagerTest, InitializerNotSet) {
  base::test::TestFuture<bool> test_future;
  boca_session_manager()->InitSessionCaption(test_future.GetCallback());
  EXPECT_TRUE(test_future.Get());
}

TEST_F(BocaSessionManagerTest, NotifyCloseCaptionsOnDeviceSessionLocked) {
  EXPECT_CALL(*observer(), OnSessionCaptionClosed(/*is_error=*/false)).Times(1);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(1);
  device_session_manger_.SetSessionState(session_manager::SessionState::LOCKED);
}

TEST_F(BocaSessionManagerTest,
       DoesNotNotifyCloseCaptionsOnDeviceSessionNotLocked) {
  EXPECT_CALL(*observer(), OnSessionCaptionClosed).Times(0);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(0);
  device_session_manger_.SetSessionState(session_manager::SessionState::ACTIVE);
}

TEST_F(BocaSessionManagerTest, UploadTokenSuccess) {
  base::HistogramTester histogram_tester;
  const std::string kFcmToken = "fcm_token";
  std::string request_fcm_token;
  base::test::TestFuture<bool> test_future;
  EXPECT_CALL(*session_client_impl(), UploadToken)
      .WillOnce(
          [&request_fcm_token](std::unique_ptr<UploadTokenRequest> request) {
            request_fcm_token = request->token();
            std::move(request->callback()).Run(true);
          });
  boca_session_manager()->UploadToken(kFcmToken, test_future.GetCallback());

  EXPECT_TRUE(test_future.Get());
  EXPECT_EQ(request_fcm_token, kFcmToken);
  histogram_tester.ExpectTotalCount(kBocaUploadTokenErrorCodeUmaPath, 0);
}

TEST_F(BocaSessionManagerTest, UploadTokenFailure) {
  base::HistogramTester histogram_tester;
  const std::string kFcmToken = "fcm_token";
  std::string request_fcm_token;
  base::test::TestFuture<bool> test_future;
  EXPECT_CALL(*session_client_impl(), UploadToken)
      .WillOnce([&request_fcm_token](
                    std::unique_ptr<UploadTokenRequest> request) {
        request_fcm_token = request->token();
        std::move(request->callback())
            .Run(base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      });
  boca_session_manager()->UploadToken(kFcmToken, test_future.GetCallback());

  EXPECT_FALSE(test_future.Get());
  EXPECT_EQ(request_fcm_token, kFcmToken);
  histogram_tester.ExpectTotalCount(kBocaUploadTokenErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaUploadTokenErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_BAD_REQUEST, 1);
}

TEST_F(BocaSessionManagerTest, OnInvalidationReceived) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*session_client_impl(), GetSession)
      .WillOnce([](std::unique_ptr<GetSessionRequest> request, bool) {
        std::move(request->callback())
            .Run(std::make_unique<::boca::Session>(
                GetInitialSession(base::Time::Now())));
      });
  EXPECT_CALL(*observer(), OnReceiverInvalidation).Times(0);
  boca_session_manager()->OnInvalidationReceived("payload");
  histogram_tester.ExpectTotalCount(boca::kPollingResult, 0);
}

TEST_F(BocaSessionManagerTest, OnReceiverInvalidationReceived) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*session_client_impl(), GetSession).Times(0);
  EXPECT_CALL(*observer(), OnReceiverInvalidation).Times(1);
  boca_session_manager()->OnInvalidationReceived("GetKioskReceiver");
}

class BocaSessionManagerSodaTest : public BocaSessionManagerTestBase {
 protected:
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeatures(
        {ash::features::kOnDeviceSpeechRecognition}, /*disabled_features=*/{});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    speech::SodaInstaller::RegisterLocalStatePrefs(local_state_registry());
    babelorca::RegisterSodaPrefsForTesting(local_state_registry());
    // Only teacher installs SODA.
    local_state().SetString(prefs::kClassManagementToolsAvailabilitySetting,
                            kTeacher);
    speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true));
    EXPECT_CALL(*boca_app_client(), GetDeviceId());
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/true);

    EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages)
        .WillRepeatedly(Return(valid_languages_));
  }

 protected:
  std::vector<std::string> valid_languages_ = {{kDefaultLanguage}};
  testing::NiceMock<babelorca::MockSodaInstaller> mock_soda_installer_;
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerSodaTest, ReturnUninstalledIfNoInstaller) {
  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, HandleSodaInstallationSuccess) {
  babelorca::SodaInstaller installer = babelorca::SodaInstaller(
      &local_state(), &local_state(), kDefaultLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());
  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kInstalling,
            boca_session_manager_->GetSodaStatus());
  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  ASSERT_EQ(BocaSessionManager::SodaStatus::kReady,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, HandleSodaBinaryInstallationFailure) {
  babelorca::SodaInstaller installer = babelorca::SodaInstaller(
      &local_state(), &local_state(), kDefaultLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());
  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kInstalling,
            boca_session_manager_->GetSodaStatus());
  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kNone);
  ASSERT_EQ(BocaSessionManager::SodaStatus::kInstallationFailure,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, HandleSodaLanguageInstallationFailure) {
  babelorca::SodaInstaller installer = babelorca::SodaInstaller(
      &local_state(), &local_state(), kDefaultLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());
  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kInstalling,
            boca_session_manager_->GetSodaStatus());
  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  ASSERT_EQ(BocaSessionManager::SodaStatus::kInstallationFailure,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, HandleUnavailableLanguage) {
  local_state().SetString(prefs::kClassManagementToolsAvailabilitySetting,
                          kTeacher);
  babelorca::SodaInstaller installer =
      babelorca::SodaInstaller(&local_state(), &local_state(), kBadLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kLanguageUnavailable,
            boca_session_manager_->GetSodaStatus());
  EXPECT_CALL(mock_soda_installer_, InstallSoda).Times(0);
  EXPECT_CALL(mock_soda_installer_, InstallLanguage).Times(0);
  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kLanguageUnavailable,
            boca_session_manager_->GetSodaStatus());
  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  ASSERT_EQ(BocaSessionManager::SodaStatus::kLanguageUnavailable,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, ListenForSuccess) {
  babelorca::SodaInstaller installer = babelorca::SodaInstaller(
      &local_state(), &local_state(), kDefaultLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());

  // On any status that's not installing immediately return the status.
  boca_session_manager_->AddObserver(observer());
  EXPECT_CALL(*observer(), OnSodaStatusUpdate).Times(1);

  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kInstalling,
            boca_session_manager_->GetSodaStatus());

  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  ASSERT_EQ(BocaSessionManager::SodaStatus::kReady,
            boca_session_manager_->GetSodaStatus());
}

TEST_F(BocaSessionManagerSodaTest, ListenForFailure) {
  babelorca::SodaInstaller installer = babelorca::SodaInstaller(
      &local_state(), &local_state(), kDefaultLanguage);
  boca_session_manager_->SetSodaInstaller(&installer);
  EXPECT_EQ(BocaSessionManager::SodaStatus::kUninstalled,
            boca_session_manager_->GetSodaStatus());
  boca_session_manager_->AddObserver(observer());
  EXPECT_CALL(*observer(), OnSodaStatusUpdate).Times(1);

  boca_session_manager_->OnAppWindowOpened();
  EXPECT_EQ(BocaSessionManager::SodaStatus::kInstalling,
            boca_session_manager_->GetSodaStatus());

  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  ASSERT_EQ(BocaSessionManager::SodaStatus::kInstallationFailure,
            boca_session_manager_->GetSodaStatus());
}

class BocaSessionManagerManagedNetworkTest : public BocaSessionManagerTestBase {
 protected:
  void SetUp() override {
    BocaSessionManagerTestBase::SetUp();
    scoped_feature_list().InitWithFeatures(
        {ash::features::kBocaNetworkRestriction},
        /*disabled_features=*/{ash::features::kBocaCustomPolling});
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/true);
    ToggleManagedNetOffline();
  }

 protected:
  const base::Time session_start_time_ = base::Time::Now();
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerManagedNetworkTest,
       DoNotLoadSessionIfNonManagedNetwork) {
  ToggleNonManagedNetOnline();
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(0);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  EXPECT_FALSE(boca_session_manager_->GetCurrentSession());
  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerManagedNetworkTest, LoadSessionWhenOnManagedNetwork) {
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
  ToggleManagedNetOnline();
  testing::Mock::VerifyAndClearExpectations(session_client_impl());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);

  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval * 1 +
                                    base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(session_client_impl());
}

TEST_F(BocaSessionManagerManagedNetworkTest,
       TriggerReloadWhenSwitchbackToManagedNetwork) {
  ToggleNonManagedNetOnline();
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
  ToggleManagedNetOnline();
  testing::Mock::VerifyAndClearExpectations(session_client_impl());

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
  task_environment()->FastForwardBy(kDefaultIndefinitePollingInterval +
                                    base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(session_client_impl());
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
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
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
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/true);
  }

 protected:
  base::Time session_start_time_ = base::Time::Now();
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerNoPollingTest, DoNotPollWhenPollingIntervalIsZero) {
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(0);
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
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
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
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/true);
  }

 private:
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerCustomPollingTest, CustomPollingInterval) {
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .Times(1);
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
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
        .WillOnce([&]() {
          // The first fetch at construction time will fail due to refresh token
          // not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        });
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    const auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/false);
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
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectTotalCount(kStudentHeartbeatErrorCodeUmaPath, 0);
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

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       StudentHeartbeatCallFailedWithRetryBackoff) {
  base::HistogramTester histogram_tester;
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_))
      .Times(3)
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR));
      }));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(
      kDefaultStudentHeartbeatInterval +
      base::Seconds(30) +        // Initial backoff delay.
      base::Seconds(30 * 1.2));  // Second backoff delay.

  histogram_tester.ExpectTotalCount(kStudentHeartbeatErrorCodeUmaPath, 3);
  histogram_tester.ExpectBucketCount(
      kStudentHeartbeatErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 3);
}

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       StudentHeartbeatCallFailedWithRetryBackoffThenSucceeded) {
  base::HistogramTester histogram_tester;
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_))
      .Times(4)
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(base::ok<bool>(true));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(base::ok<bool>(true));
      }));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(
      kDefaultStudentHeartbeatInterval +
      base::Seconds(30) +        // Initial backoff delay.
      base::Seconds(30 * 1.2) +  // Second backoff delay.
      base::Seconds(30));        // Default heartbeat interval.

  histogram_tester.ExpectTotalCount(kStudentHeartbeatErrorCodeUmaPath, 2);
  histogram_tester.ExpectBucketCount(
      kStudentHeartbeatErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 2);
}

TEST_F(BocaSessionManagerStudentHeartbeatTest,
       StudentHeartbeatCallFailedWithRetryBackoffWithNewSession) {
  base::HistogramTester histogram_tester;
  ::boca::Session session_1;
  session_1.set_session_id(kInitialSessionId);
  session_1.set_session_state(::boca::Session::ACTIVE);
  session_1.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_1.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  EXPECT_CALL(*session_client_impl(), StudentHeartbeat(_))
      .Times(3)
      .WillRepeatedly(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->OnStudentHeartbeat(
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR));
      }));

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_1), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(
      kDefaultStudentHeartbeatInterval +
      base::Seconds(30));  // Second backoff delay.

  // Start another session
  ::boca::Session session_2;
  session_2.set_session_state(::boca::Session::ACTIVE);
  session_2.set_session_id(kInitialSessionId);
  session_2.mutable_duration()->set_seconds(kInitialSessionDurationInSecs);
  session_2.mutable_start_time()->set_seconds(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  boca_session_manager()->UpdateCurrentSession(
      std::make_unique<::boca::Session>(session_2), /*dispatch_event=*/true);

  task_environment()->FastForwardBy(base::Seconds(30 * 1.2));
  histogram_tester.ExpectTotalCount(kStudentHeartbeatErrorCodeUmaPath, 3);
  histogram_tester.ExpectBucketCount(
      kStudentHeartbeatErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 3);
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
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
        .WillOnce([&]() {
          // The first fetch at construction time will fail due to refresh token
          // not ready.
          boca_session_manager_->ParseSessionResponse(
              /*from_polling=*/false,
              base::unexpected<google_apis::ApiErrorCode>(
                  google_apis::ApiErrorCode::NOT_READY));
        });
    EXPECT_CALL(*boca_app_client(), GetDeviceId())
        .WillRepeatedly(Return(kDeviceId));
    const auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/false);
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
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
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
        session_client_impl(), &local_state(), account_id,
        /*is_producer=*/false);
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

class BocaSessionManagerConsumerTest : public BocaSessionManagerTest {
 protected:
  void SetUp() override {
    is_producer_ = false;
    BocaSessionManagerTest::SetUp();
  }
};

TEST_F(BocaSessionManagerConsumerTest,
       NotifyConsumerSessionUpdateWhenSessionCaptionUpdated) {
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

  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/true))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(/*from_polling=*/false,
                                                     std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionCaptionConfigUpdated).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(kDefaultInSessionPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerConsumerTest,
       NotifyCloseLocalCaptionsOnlyOnDeviceSessionLocked) {
  EXPECT_CALL(*observer(), OnSessionCaptionClosed).Times(0);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(1);
  device_session_manger_.SetSessionState(session_manager::SessionState::LOCKED);
}

TEST_F(BocaSessionManagerConsumerTest,
       NotifyCloseLocalCaptionsOnlyOnActiveUserChanged) {
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kTestUserEmail2, kTestGaiaId2);
  const std::string username_hash =
      user_manager::TestHelper::GetFakeUsernameHash(account_id);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(account_id, username_hash);
  testing::Mock::VerifyAndClearExpectations(session_client_impl());
  EXPECT_CALL(*observer(), OnSessionCaptionClosed).Times(0);
  EXPECT_CALL(*observer(), OnLocalCaptionClosed).Times(1);
  user_manager::UserManager::Get()->SwitchActiveUser(
      AccountId::FromUserEmailGaiaId(kTestUserEmail2, kTestGaiaId2));
}

}  // namespace
}  // namespace ash::boca
