// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_status_processor.h"

#include <google/protobuf/repeated_field.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/fake_do_not_disturb_controller.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/fake_find_my_device_controller.h"
#include "chromeos/ash/components/phonehub/fake_message_receiver.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/fake_notification_manager.h"
#include "chromeos/ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/fake_screen_lock_manager.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/icon_decoder_impl.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"
#include "chromeos/ash/components/phonehub/notification_processor.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/components/phonehub/phone_status_model.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace ash::phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

namespace {

constexpr auto kLatencyDelta = base::Milliseconds(456u);
constexpr char kAppListUpdateLatencyHistogramName[] =
    "Eche.AppListUpdate.Latency";

}  // namespace

// A fake processor that immediately adds or removes notifications.
class FakeNotificationProcessor : public NotificationProcessor {
 public:
  FakeNotificationProcessor(NotificationManager* notification_manager)
      : NotificationProcessor(notification_manager) {}

  void AddNotifications(
      const std::vector<proto::Notification>& notification_protos) override {
    base::flat_set<Notification> notifications;
    for (const auto& proto : notification_protos) {
      notifications.emplace(Notification(
          proto.id(), CreateFakeAppMetadata(), base::Time(),
          Notification::Importance::kDefault,
          Notification::Category::kConversation,
          {{Notification::ActionType::kInlineReply, /*action_id=*/0}},
          Notification::InteractionBehavior::kNone, std::nullopt, std::nullopt,
          std::nullopt, std::nullopt));
    }
    notification_manager_->SetNotificationsInternal(notifications);
  }

  void RemoveNotifications(
      const base::flat_set<int64_t>& notification_ids) override {
    notification_manager_->RemoveNotificationsInternal(notification_ids);
  }
};

class AppStreamManagerObserver : public AppStreamManager::Observer {
 public:
  void OnAppStreamUpdate(
      const proto::AppStreamUpdate app_stream_update) override {
    last_app_stream_update_ = app_stream_update.foreground_app().package_name();
  }

  std::string last_app_stream_update_;
};

class PhoneStatusProcessorTest : public testing::Test {
  class TestDecoderDelegate : public IconDecoderImpl::DecoderDelegate {
   public:
    TestDecoderDelegate() = default;
    ~TestDecoderDelegate() override = default;

    void Decode(const IconDecoder::DecodingData& data,
                data_decoder::DecodeImageCallback callback) override {
      pending_callbacks_[data.id] = std::move(callback);
      CompleteRequest(data.id);
    }

    void CompleteRequest(const unsigned long id) {
      SkBitmap test_bitmap;
      test_bitmap.allocN32Pixels(id % 10, 1);
      std::move(pending_callbacks_.at(id)).Run(test_bitmap);
      pending_callbacks_.erase(id);
    }

    void FailRequest(const unsigned long id) {
      SkBitmap test_bitmap;
      std::move(pending_callbacks_.at(id)).Run(test_bitmap);
      pending_callbacks_.erase(id);
    }

    void CompleteAllRequests() {
      for (auto& it : pending_callbacks_)
        CompleteRequest(it.first);
      pending_callbacks_.clear();
    }

   private:
    base::flat_map<unsigned long, data_decoder::DecodeImageCallback>
        pending_callbacks_;
  };

 protected:
  PhoneStatusProcessorTest()
      : test_remote_device_(multidevice::CreateRemoteDeviceRefForTest()) {}
  PhoneStatusProcessorTest(const PhoneStatusProcessorTest&) = delete;
  PhoneStatusProcessorTest& operator=(const PhoneStatusProcessorTest&) = delete;
  ~PhoneStatusProcessorTest() override = default;

  void SetUp() override {
    PhoneHubStructuredMetricsLogger::RegisterPrefs(pref_service_.registry());
    multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kPhoneHubCameraRoll},
        /*disabled_features=*/{features::kEcheLauncher});

    fake_do_not_disturb_controller_ =
        std::make_unique<FakeDoNotDisturbController>();
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>(FeatureStatus::kDisabled);
    fake_message_receiver_ = std::make_unique<FakeMessageReceiver>();
    fake_find_my_device_controller_ =
        std::make_unique<FakeFindMyDeviceController>();
    fake_multidevice_feature_access_manager_ =
        std::make_unique<FakeMultideviceFeatureAccessManager>();
    fake_screen_lock_manager_ = std::make_unique<FakeScreenLockManager>();
    fake_notification_manager_ = std::make_unique<FakeNotificationManager>();
    fake_notification_processor_ = std::make_unique<FakeNotificationProcessor>(
        fake_notification_manager_.get());
    mutable_phone_model_ = std::make_unique<MutablePhoneModel>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_recent_apps_interaction_handler_ =
        std::make_unique<FakeRecentAppsInteractionHandler>();
    icon_decoder_ = std::make_unique<IconDecoderImpl>();
    icon_decoder_.get()->decoder_delegate_ =
        std::make_unique<TestDecoderDelegate>();
    decoder_delegate_ = static_cast<TestDecoderDelegate*>(
        icon_decoder_.get()->decoder_delegate_.get());
    app_stream_launcher_data_model_ =
        std::make_unique<AppStreamLauncherDataModel>();
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    phone_hub_ui_readiness_recorder_ =
        std::make_unique<PhoneHubUiReadinessRecorder>(
            fake_feature_status_provider_.get(),
            fake_connection_manager_.get());
    phone_hub_structured_metrics_logger_ =
        std::make_unique<PhoneHubStructuredMetricsLogger>(&pref_service_);
  }

  void CreatePhoneStatusProcessor() {
    phone_status_processor_ = std::make_unique<PhoneStatusProcessor>(
        fake_do_not_disturb_controller_.get(),
        fake_feature_status_provider_.get(), fake_message_receiver_.get(),
        fake_find_my_device_controller_.get(),
        fake_multidevice_feature_access_manager_.get(),
        fake_screen_lock_manager_.get(), fake_notification_processor_.get(),
        fake_multidevice_setup_client_.get(), mutable_phone_model_.get(),
        fake_recent_apps_interaction_handler_.get(), &pref_service_,
        &app_stream_manager_, app_stream_launcher_data_model_.get(),
        icon_decoder_.get(), phone_hub_ui_readiness_recorder_.get(),
        phone_hub_structured_metrics_logger_.get());
  }

  void InitializeNotificationProto(proto::Notification* notification,
                                   int64_t id) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_package_name("package");
    origin_app->set_visible_name("visible");
    origin_app->set_icon("321");

    notification->add_actions();
    proto::Action* mutable_action = notification->mutable_actions(0);
    mutable_action->set_id(0u);
    mutable_action->set_title("action title");
    mutable_action->set_type(proto::Action_InputType::Action_InputType_TEXT);

    notification->set_id(id);
    notification->set_epoch_time_millis(1u);
    notification->set_allocated_origin_app(origin_app.release());
    notification->set_title("title");
    notification->set_importance(proto::NotificationImportance::HIGH);
    notification->set_text_content("content");
    notification->set_contact_image("123");
    notification->set_shared_image("123");
  }

  ash::multidevice_setup::EcheSupportReceivedFromPhoneHub
  GetEcheSupportReceivedFromPhoneHub() {
    return static_cast<ash::multidevice_setup::EcheSupportReceivedFromPhoneHub>(
        pref_service_.GetInteger(
            ash::multidevice_setup::
                kEcheOverriddenSupportReceivedFromPhoneHubPrefName));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  multidevice::RemoteDeviceRef test_remote_device_;
  std::unique_ptr<FakeDoNotDisturbController> fake_do_not_disturb_controller_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageReceiver> fake_message_receiver_;
  std::unique_ptr<FakeFindMyDeviceController> fake_find_my_device_controller_;
  std::unique_ptr<FakeMultideviceFeatureAccessManager>
      fake_multidevice_feature_access_manager_;
  std::unique_ptr<FakeScreenLockManager> fake_screen_lock_manager_;
  std::unique_ptr<FakeNotificationManager> fake_notification_manager_;
  std::unique_ptr<FakeNotificationProcessor> fake_notification_processor_;
  std::unique_ptr<MutablePhoneModel> mutable_phone_model_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeRecentAppsInteractionHandler>
      fake_recent_apps_interaction_handler_;
  std::unique_ptr<IconDecoderImpl> icon_decoder_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<PhoneHubUiReadinessRecorder> phone_hub_ui_readiness_recorder_;
  std::unique_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_;
  raw_ptr<TestDecoderDelegate> decoder_delegate_;
  TestingPrefServiceSimple pref_service_;
  AppStreamManager app_stream_manager_;
  AppStreamManagerObserver app_stream_manager_observer_;
  std::unique_ptr<AppStreamLauncherDataModel> app_stream_launcher_data_model_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
};

TEST_F(PhoneStatusProcessorTest, PhoneStatusSnapshotUpdate_EcheDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEcheSWA,
                             features::kPhoneHubCameraRoll});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_NOT_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_UNKNOWN);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(true);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0),
                              /*id=*/0u);
  auto* app = expected_snapshot.mutable_streamable_apps()->add_apps();
  app->set_package_name("pkg1");
  app->set_visible_name("vis");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kUnknown,
            fake_screen_lock_manager_->GetLockStatus());

  std::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_TRUE(user_states.empty());
}

TEST_F(PhoneStatusProcessorTest, PhoneStatusSnapshotUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_NOT_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_UNKNOWN);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(true);
  expected_phone_properties->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_SUPPORTED);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0),
                              /*id=*/0u);
  auto* app = expected_snapshot.mutable_streamable_apps()->add_apps();
  app->set_package_name("pkg1");
  app->set_visible_name("vis");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kUnknown,
            fake_screen_lock_manager_->GetLockStatus());

  std::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_EQ(1u, user_states[0].user_id);
  EXPECT_EQ(true, user_states[0].is_enabled);

  EXPECT_TRUE(app_stream_launcher_data_model_->GetAppsList()->empty());
}

TEST_F(PhoneStatusProcessorTest,
       PhoneStatusSnapshotUpdate_AppStreamLauncher_enabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_NOT_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_UNKNOWN);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(true);
  expected_phone_properties->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_SUPPORTED);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0),
                              /*id=*/0u);

  auto* streamable_apps = expected_snapshot.mutable_streamable_apps();
  auto* app = streamable_apps->add_apps();
  app->set_package_name("pkg1");
  app->set_visible_name("vis");

  auto* app2 = streamable_apps->add_apps();
  app2->set_package_name("pkg2");
  app2->set_visible_name("a_vis");  // Test alphbetical sort.

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  proto::AppListUpdate expected_all_apps;

  auto* all_apps = expected_all_apps.mutable_all_apps();
  auto* all_app1 = all_apps->add_apps();
  all_app1->set_package_name("pkg1");
  all_app1->set_visible_name("vis");

  auto* all_app2 = all_apps->add_apps();
  all_app2->set_package_name("pkg2");
  all_app2->set_visible_name("a_vis");  // Test alphbetical sort.

  fake_message_receiver_->NotifyAppListUpdateReceived(expected_all_apps);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kUnknown,
            fake_screen_lock_manager_->GetLockStatus());

  std::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_EQ(1u, user_states[0].user_id);
  EXPECT_EQ(true, user_states[0].is_enabled);

  EXPECT_EQ(2u,
            app_stream_launcher_data_model_->GetAppsListSortedByName()->size());
  EXPECT_EQ(u"a_vis", app_stream_launcher_data_model_->GetAppsListSortedByName()
                          ->at(0)
                          .visible_app_name);
  EXPECT_EQ(u"vis", app_stream_launcher_data_model_->GetAppsListSortedByName()
                        ->at(1)
                        .visible_app_name);

  EXPECT_EQ(2u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(u"vis",
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .at(0)
                .visible_app_name);
  EXPECT_EQ(u"a_vis",
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .at(1)
                .visible_app_name);
}

TEST_F(PhoneStatusProcessorTest, PhoneStatusUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(proto::ProfileType::WORK_PROFILE);
  expected_phone_properties->set_find_my_device_capability(
      proto::FindMyDeviceCapability::NOT_ALLOWED);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::NOT_RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_OFF);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(false);
  expected_phone_properties->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_SUPPORTED);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(0),
                              /*id=*/0u);

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kLockedOff,
            fake_screen_lock_manager_->GetLockStatus());

  std::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_EQ(1u, user_states[0].user_id);
  EXPECT_EQ(true, user_states[0].is_enabled);

  // Update with one removed notification and a default profile.
  expected_update.add_removed_notification_ids(0u);
  expected_update.mutable_properties()->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_update.mutable_properties()->set_find_my_device_capability(
      proto::FindMyDeviceCapability::NORMAL);
  expected_update.mutable_properties()->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_update.mutable_properties()->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_ON);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_EQ(ScreenLockManager::LockStatus::kLockedOn,
            fake_screen_lock_manager_->GetLockStatus());

  phone_status_model = mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());
}

TEST_F(PhoneStatusProcessorTest, PhoneNotificationAccessProhibitedReason) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_profile_type(proto::ProfileType::WORK_PROFILE);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);

  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile,
      fake_multidevice_feature_access_manager_
          ->GetNotificationAccessProhibitedReason());

  // Verify that adding a reason properly gets processed even when the current
  // profile type does not change.
  expected_update.mutable_properties()->set_profile_disable_reason(
      proto::ProfileDisableReason::DISABLE_REASON_DISABLED_BY_POLICY);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile,
      fake_multidevice_feature_access_manager_
          ->GetNotificationAccessProhibitedReason());
}

TEST_F(PhoneStatusProcessorTest, PhoneName) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, std::nullopt));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(std::nullopt, mutable_phone_model_->phone_name());

  // Create new fake phone with name.
  const multidevice::RemoteDeviceRef kFakePhoneA =
      multidevice::RemoteDeviceRefBuilder().SetName("Phone A").Build();

  // Trigger a host status update and expect a new phone with new name to be
  // updated.
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, kFakePhoneA));

  EXPECT_EQ(u"Phone A", mutable_phone_model_->phone_name());
}

TEST_F(PhoneStatusProcessorTest, NotificationAccess) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  proto::PhoneStatusUpdate expected_update;

  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(0),
                              /*id=*/0u);

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());

  // Simulate notifications feature state set as disabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);
  // Update with 1 new notification, expect no notifications to be processed.
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(1),
                              /*id=*/1u);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());

  // Re-enable notifications and expect the previous notification to be now
  // added.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(2u, fake_notification_manager_->num_notifications());
}

TEST_F(PhoneStatusProcessorTest, EcheFeatureStatus) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_UNSPECIFIED);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(
      ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSpecified,
      GetEcheSupportReceivedFromPhoneHub());

  expected_update.mutable_properties()->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_SUPPORTED);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  expected_update.mutable_properties()->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_ENABLED);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  expected_update.mutable_properties()->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_PROHIBITED_BY_POLICY);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported,
            GetEcheSupportReceivedFromPhoneHub());

  expected_update.mutable_properties()->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_UNSUPPORTED);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(
      ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSupported,
      GetEcheSupportReceivedFromPhoneHub());

  expected_update.mutable_properties()->set_eche_feature_status(
      proto::FeatureStatus::FEATURE_STATUS_ATTESTATION_FAILED);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(
      ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSupported,
      GetEcheSupportReceivedFromPhoneHub());
}

TEST_F(PhoneStatusProcessorTest, OnAppStreamUpdateReceived) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto app = std::make_unique<proto::App>();
  app->set_package_name("app1");
  app->set_visible_name("vis1");
  app->set_icon("icon1");

  proto::AppStreamUpdate expected_update;
  expected_update.set_allocated_foreground_app(app.release());

  app_stream_manager_.AddObserver(&app_stream_manager_observer_);

  fake_message_receiver_->NotifyAppStreamUpdateReceived(expected_update);

  EXPECT_EQ("app1", app_stream_manager_observer_.last_app_stream_update_);
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateReceived_allApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  auto* app2 = streamable_apps->add_apps();
  app2->set_package_name("pkg2");
  app2->set_visible_name("second_app");
  app2->set_icon("icon2");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(0u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(2u, app_stream_launcher_data_model_->GetAppsList()->size());
  EXPECT_EQ(
      u"first_app",
      app_stream_launcher_data_model_->GetAppsList()->at(0).visible_app_name);
  EXPECT_EQ(
      u"second_app",
      app_stream_launcher_data_model_->GetAppsList()->at(1).visible_app_name);
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateReceived_recentApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_recent_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  auto* app2 = streamable_apps->add_apps();
  app2->set_package_name("pkg2");
  app2->set_visible_name("second_app");
  app2->set_icon("icon2");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(0u, app_stream_launcher_data_model_->GetAppsList()->size());
  EXPECT_EQ(2u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(u"first_app",
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .at(0)
                .visible_app_name);
  EXPECT_EQ(u"second_app",
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .at(1)
                .visible_app_name);
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll},
      /*disabled_features=*/{features::kEcheLauncher});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  auto* app2 = streamable_apps->add_apps();
  app2->set_package_name("pkg2");
  app2->set_visible_name("second_app");
  app2->set_icon("icon2");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);

  EXPECT_EQ(0u, app_stream_launcher_data_model_->GetAppsList()->size());
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateNoApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate expected_update;

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);

  EXPECT_EQ(0u, app_stream_launcher_data_model_->GetAppsList()->size());
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateLatency) {
  base::HistogramTester histogram_tester;

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  proto::PhoneStatusSnapshot expected_snapshot;
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  task_environment_.FastForwardBy(kLatencyDelta);

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);

  histogram_tester.ExpectTimeBucketCount(kAppListUpdateLatencyHistogramName,
                                         kLatencyDelta, 1);
  EXPECT_EQ(1u, app_stream_launcher_data_model_->GetAppsList()->size());
}

TEST_F(PhoneStatusProcessorTest, OnAppListUpdateLatencyFlagDisabled) {
  base::HistogramTester histogram_tester;

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll},
      /*disabled_features=*/{features::kEcheLauncher});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  // Simulate receiving a proto message.
  proto::PhoneStatusSnapshot expected_snapshot;
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  task_environment_.FastForwardBy(kLatencyDelta);

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);

  histogram_tester.ExpectTimeBucketCount(kAppListUpdateLatencyHistogramName,
                                         kLatencyDelta, 0);
  EXPECT_EQ(0u, app_stream_launcher_data_model_->GetAppsList()->size());
}

TEST_F(PhoneStatusProcessorTest,
       OnAppListIncrementalUpdateReceived_installApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate expected_update;
  auto* streamable_apps = expected_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(expected_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(0u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(1u, app_stream_launcher_data_model_->GetAppsList()->size());
  EXPECT_EQ(
      u"first_app",
      app_stream_launcher_data_model_->GetAppsList()->at(0).visible_app_name);

  proto::AppListIncrementalUpdate incremental_update;
  auto* installed_apps = incremental_update.mutable_installed_apps();
  auto* installed_app = installed_apps->add_apps();
  installed_app->set_package_name("pkg2");
  installed_app->set_visible_name("second_app");
  installed_app->set_icon("icon2");

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListIncrementalUpdateReceived(
      incremental_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(0u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(2u, app_stream_launcher_data_model_->GetAppsList()->size());
  EXPECT_EQ(
      u"second_app",
      app_stream_launcher_data_model_->GetAppsList()->at(1).visible_app_name);
}

TEST_F(PhoneStatusProcessorTest,
       OnAppListIncrementalUpdateReceived_removeApps) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kEcheLauncher},
      /*disabled_features=*/{});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  proto::AppListUpdate list_update;
  auto* streamable_apps = list_update.mutable_all_apps();
  auto* app1 = streamable_apps->add_apps();
  app1->set_package_name("pkg1");
  app1->set_visible_name("first_app");
  app1->set_icon("icon1");

  auto* app2 = streamable_apps->add_apps();
  app2->set_package_name("pkg2");
  app2->set_visible_name("second_app");
  app2->set_icon("icon2");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListUpdateReceived(list_update);
  decoder_delegate_->CompleteAllRequests();
  EXPECT_EQ(2u, app_stream_launcher_data_model_->GetAppsList()->size());

  proto::AppListUpdate recent_app_update;
  auto* recent_apps = recent_app_update.mutable_recent_apps();
  auto* recent_app1 = recent_apps->add_apps();
  recent_app1->set_package_name("pkg1");
  recent_app1->set_visible_name("first_app");
  recent_app1->set_icon("icon1");

  auto* recent_app2 = recent_apps->add_apps();
  recent_app2->set_package_name("pkg2");
  recent_app2->set_visible_name("second_app");
  recent_app2->set_icon("icon2");

  fake_message_receiver_->NotifyAppListUpdateReceived(recent_app_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(2u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());

  proto::AppListIncrementalUpdate incremental_update;
  auto* removed_apps = incremental_update.mutable_removed_apps();
  auto* removed_app = removed_apps->add_apps();
  removed_app->set_package_name("pkg2");
  removed_app->set_visible_name("second_app");
  removed_app->set_icon("icon2");

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyAppListIncrementalUpdateReceived(
      incremental_update);
  decoder_delegate_->CompleteAllRequests();

  EXPECT_EQ(1u, app_stream_launcher_data_model_->GetAppsList()->size());
  EXPECT_EQ(
      u"first_app",
      app_stream_launcher_data_model_->GetAppsList()->at(0).visible_app_name);
  EXPECT_EQ(1u,
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .size());
  EXPECT_EQ(u"first_app",
            fake_recent_apps_interaction_handler_->FetchRecentAppMetadataList()
                .at(0)
                .visible_app_name);
}

}  // namespace ash::phonehub
