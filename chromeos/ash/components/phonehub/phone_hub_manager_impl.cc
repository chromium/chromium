// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_manager_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/system_info_provider.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_controller.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager_impl.h"
#include "chromeos/ash/components/phonehub/connection_scheduler_impl.h"
#include "chromeos/ash/components/phonehub/cros_state_sender.h"
#include "chromeos/ash/components/phonehub/do_not_disturb_controller_impl.h"
#include "chromeos/ash/components/phonehub/feature_status_provider_impl.h"
#include "chromeos/ash/components/phonehub/find_my_device_controller_impl.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/icon_decoder_impl.h"
#include "chromeos/ash/components/phonehub/invalid_connection_disconnector.h"
#include "chromeos/ash/components/phonehub/message_receiver_impl.h"
#include "chromeos/ash/components/phonehub/message_sender_impl.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager_impl.h"
#include "chromeos/ash/components/phonehub/multidevice_setup_state_updater.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/notification_interaction_handler_impl.h"
#include "chromeos/ash/components/phonehub/notification_manager_impl.h"
#include "chromeos/ash/components/phonehub/notification_processor.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker_impl.h"
#include "chromeos/ash/components/phonehub/phone_hub_metrics_recorder.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "chromeos/ash/components/phonehub/phone_status_processor.h"
#include "chromeos/ash/components/phonehub/ping_manager_impl.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler_impl.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager_impl.h"
#include "chromeos/ash/components/phonehub/tether_controller_impl.h"
#include "chromeos/ash/components/phonehub/user_action_recorder_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {
namespace phonehub {

namespace {

const char kSecureChannelFeatureName[] = "phone_hub";

}  // namespace

PhoneHubManagerImpl::PhoneHubManagerImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider,
    std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager,
    const base::RepeatingClosure& show_multidevice_setup_dialog_callback,
    std::unique_ptr<AttestationCertificateGenerator>
        attestation_certificate_generator)
    : icon_decoder_(std::make_unique<IconDecoderImpl>()),
      phone_hub_structured_metrics_logger_(
          std::make_unique<PhoneHubStructuredMetricsLogger>(pref_service)),
      connection_manager_(
          std::make_unique<secure_channel::ConnectionManagerImpl>(
              multidevice_setup_client,
              device_sync_client,
              secure_channel_client,
              kSecureChannelFeatureName,
              std::make_unique<PhoneHubMetricsRecorder>(),
              phone_hub_structured_metrics_logger_.get())),
      feature_status_provider_(std::make_unique<FeatureStatusProviderImpl>(
          device_sync_client,
          multidevice_setup_client,
          connection_manager_.get(),
          session_manager::SessionManager::Get(),
          chromeos::PowerManagerClient::Get(),
          phone_hub_structured_metrics_logger_.get())),
      user_action_recorder_(std::make_unique<UserActionRecorderImpl>(
          feature_status_provider_.get())),
      phone_hub_ui_readiness_recorder_(
          std::make_unique<PhoneHubUiReadinessRecorder>(
              feature_status_provider_.get(),
              connection_manager_.get())),
      message_receiver_(std::make_unique<MessageReceiverImpl>(
          connection_manager_.get(),
          phone_hub_structured_metrics_logger_.get())),
      message_sender_(std::make_unique<MessageSenderImpl>(
          connection_manager_.get(),
          phone_hub_ui_readiness_recorder_.get(),
          phone_hub_structured_metrics_logger_.get())),
      phone_model_(std::make_unique<MutablePhoneModel>()),
      cros_state_sender_(std::make_unique<CrosStateSender>(
          message_sender_.get(),
          connection_manager_.get(),
          multidevice_setup_client,
          phone_model_.get(),
          std::move(attestation_certificate_generator))),
      do_not_disturb_controller_(std::make_unique<DoNotDisturbControllerImpl>(
          message_sender_.get(),
          user_action_recorder_.get())),
      connection_scheduler_(std::make_unique<ConnectionSchedulerImpl>(
          connection_manager_.get(),
          feature_status_provider_.get(),
          phone_hub_structured_metrics_logger_.get())),
      find_my_device_controller_(std::make_unique<FindMyDeviceControllerImpl>(
          message_sender_.get(),
          user_action_recorder_.get())),
      multidevice_feature_access_manager_(
          std::make_unique<MultideviceFeatureAccessManagerImpl>(
              pref_service,
              multidevice_setup_client,
              feature_status_provider_.get(),
              message_sender_.get(),
              connection_scheduler_.get())),
      screen_lock_manager_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<ScreenLockManagerImpl>(pref_service)
              : nullptr),
      notification_interaction_handler_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<NotificationInteractionHandlerImpl>()
              : nullptr),
      notification_manager_(
          std::make_unique<NotificationManagerImpl>(message_sender_.get(),
                                                    user_action_recorder_.get(),
                                                    multidevice_setup_client)),
      onboarding_ui_tracker_(std::make_unique<OnboardingUiTrackerImpl>(
          pref_service,
          feature_status_provider_.get(),
          multidevice_setup_client,
          show_multidevice_setup_dialog_callback)),
      app_stream_launcher_data_model_(
          std::make_unique<AppStreamLauncherDataModel>()),
      notification_processor_(
          std::make_unique<NotificationProcessor>(notification_manager_.get())),
      recent_apps_interaction_handler_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<RecentAppsInteractionHandlerImpl>(
                    pref_service,
                    multidevice_setup_client,
                    multidevice_feature_access_manager_.get())
              : nullptr),
      app_stream_manager_(std::make_unique<AppStreamManager>()),
      phone_status_processor_(std::make_unique<PhoneStatusProcessor>(
          do_not_disturb_controller_.get(),
          feature_status_provider_.get(),
          message_receiver_.get(),
          find_my_device_controller_.get(),
          multidevice_feature_access_manager_.get(),
          screen_lock_manager_.get(),
          notification_processor_.get(),
          multidevice_setup_client,
          phone_model_.get(),
          recent_apps_interaction_handler_.get(),
          pref_service,
          app_stream_manager_.get(),
          app_stream_launcher_data_model_.get(),
          icon_decoder_.get(),
          phone_hub_ui_readiness_recorder_.get(),
          phone_hub_structured_metrics_logger_.get())),
      tether_controller_(
          std::make_unique<TetherControllerImpl>(phone_model_.get(),
                                                 user_action_recorder_.get(),
                                                 multidevice_setup_client)),
      browser_tabs_model_provider_(std::move(browser_tabs_model_provider)),
      browser_tabs_model_controller_(
          std::make_unique<BrowserTabsModelController>(
              multidevice_setup_client,
              browser_tabs_model_provider_.get(),
              phone_model_.get())),
      multidevice_setup_state_updater_(
          std::make_unique<MultideviceSetupStateUpdater>(
              pref_service,
              multidevice_setup_client,
              multidevice_feature_access_manager_.get())),
      invalid_connection_disconnector_(
          std::make_unique<InvalidConnectionDisconnector>(
              connection_manager_.get(),
              phone_model_.get())),
      camera_roll_manager_(features::IsPhoneHubCameraRollEnabled()
                               ? std::make_unique<CameraRollManagerImpl>(
                                     message_receiver_.get(),
                                     message_sender_.get(),
                                     multidevice_setup_client,
                                     connection_manager_.get(),
                                     std::move(camera_roll_download_manager))
                               : nullptr),
      feature_setup_response_processor_(
          features::IsPhoneHubFeatureSetupErrorHandlingEnabled()
              ? std::make_unique<FeatureSetupResponseProcessor>(
                    message_receiver_.get(),
                    multidevice_feature_access_manager_.get())
              : nullptr),
      ping_manager_(features::IsPhoneHubPingOnBubbleOpenEnabled()
                        ? std::make_unique<PingManagerImpl>(
                              connection_manager_.get(),
                              feature_status_provider_.get(),
                              message_receiver_.get(),
                              message_sender_.get())
                        : nullptr) {}

PhoneHubManagerImpl::~PhoneHubManagerImpl() = default;

BrowserTabsModelProvider* PhoneHubManagerImpl::GetBrowserTabsModelProvider() {
  return browser_tabs_model_provider_.get();
}

CameraRollManager* PhoneHubManagerImpl::GetCameraRollManager() {
  return camera_roll_manager_.get();
}

ConnectionScheduler* PhoneHubManagerImpl::GetConnectionScheduler() {
  return connection_scheduler_.get();
}

DoNotDisturbController* PhoneHubManagerImpl::GetDoNotDisturbController() {
  return do_not_disturb_controller_.get();
}

FeatureStatusProvider* PhoneHubManagerImpl::GetFeatureStatusProvider() {
  return feature_status_provider_.get();
}

FindMyDeviceController* PhoneHubManagerImpl::GetFindMyDeviceController() {
  return find_my_device_controller_.get();
}

MultideviceFeatureAccessManager*
PhoneHubManagerImpl::GetMultideviceFeatureAccessManager() {
  return multidevice_feature_access_manager_.get();
}

NotificationInteractionHandler*
PhoneHubManagerImpl::GetNotificationInteractionHandler() {
  return notification_interaction_handler_.get();
}

NotificationManager* PhoneHubManagerImpl::GetNotificationManager() {
  return notification_manager_.get();
}

OnboardingUiTracker* PhoneHubManagerImpl::GetOnboardingUiTracker() {
  return onboarding_ui_tracker_.get();
}

AppStreamLauncherDataModel*
PhoneHubManagerImpl::GetAppStreamLauncherDataModel() {
  return app_stream_launcher_data_model_.get();
}

PhoneModel* PhoneHubManagerImpl::GetPhoneModel() {
  return phone_model_.get();
}

PingManager* PhoneHubManagerImpl::GetPingManager() {
  return ping_manager_.get();
}

RecentAppsInteractionHandler*
PhoneHubManagerImpl::GetRecentAppsInteractionHandler() {
  return recent_apps_interaction_handler_.get();
}

ScreenLockManager* PhoneHubManagerImpl::GetScreenLockManager() {
  return screen_lock_manager_.get();
}

TetherController* PhoneHubManagerImpl::GetTetherController() {
  return tether_controller_.get();
}

UserActionRecorder* PhoneHubManagerImpl::GetUserActionRecorder() {
  return user_action_recorder_.get();
}

IconDecoder* PhoneHubManagerImpl::GetIconDecoder() {
  return icon_decoder_.get();
}

AppStreamManager* PhoneHubManagerImpl::GetAppStreamManager() {
  return app_stream_manager_.get();
}

PhoneHubUiReadinessRecorder*
PhoneHubManagerImpl::GetPhoneHubUiReadinessRecorder() {
  return phone_hub_ui_readiness_recorder_.get();
}

void PhoneHubManagerImpl::GetHostLastSeenTimestamp(
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  connection_manager_->GetHostLastSeenTimestamp(std::move(callback));
}

eche_app::EcheConnectionStatusHandler*
PhoneHubManagerImpl::GetEcheConnectionStatusHandler() {
  return eche_connection_status_handler_;
}

void PhoneHubManagerImpl::SetEcheConnectionStatusHandler(
    eche_app::EcheConnectionStatusHandler* eche_connection_status_handler) {
  eche_connection_status_handler_ = eche_connection_status_handler;
  recent_apps_interaction_handler_->SetConnectionStatusHandler(
      eche_connection_status_handler_);
}

void PhoneHubManagerImpl::SetSystemInfoProvider(
    eche_app::SystemInfoProvider* system_info_provider) {
  system_info_provider_ = system_info_provider;
}

eche_app::SystemInfoProvider* PhoneHubManagerImpl::GetSystemInfoProvider() {
  return system_info_provider_;
}

PhoneHubStructuredMetricsLogger*
PhoneHubManagerImpl::GetPhoneHubStructuredMetricsLogger() {
  return phone_hub_structured_metrics_logger_.get();
}

// NOTE: These should be destroyed in the opposite order of how these objects
// are initialized in the constructor.
void PhoneHubManagerImpl::Shutdown() {
  ping_manager_.reset();
  feature_setup_response_processor_.reset();
  camera_roll_manager_.reset();
  invalid_connection_disconnector_.reset();
  multidevice_setup_state_updater_.reset();
  browser_tabs_model_controller_.reset();
  browser_tabs_model_provider_.reset();
  tether_controller_.reset();
  phone_status_processor_.reset();
  recent_apps_interaction_handler_.reset();
  notification_processor_.reset();
  onboarding_ui_tracker_.reset();
  notification_manager_.reset();
  notification_interaction_handler_.reset();
  screen_lock_manager_.reset();
  multidevice_feature_access_manager_.reset();
  find_my_device_controller_.reset();
  connection_scheduler_.reset();
  do_not_disturb_controller_.reset();
  cros_state_sender_.reset();
  phone_model_.reset();
  message_sender_.reset();
  message_receiver_.reset();
  phone_hub_ui_readiness_recorder_.reset();
  user_action_recorder_.reset();
  feature_status_provider_.reset();
  connection_manager_.reset();
  phone_hub_structured_metrics_logger_.reset();
}

}  // namespace phonehub
}  // namespace ash
