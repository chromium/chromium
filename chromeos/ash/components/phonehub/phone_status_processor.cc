// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_status_processor.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/do_not_disturb_controller.h"
#include "chromeos/ash/components/phonehub/find_my_device_controller.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/icon_decoder_impl.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/notification_processor.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::MultiDeviceSetupClient;

PhoneStatusModel::MobileStatus GetMobileStatusFromProto(
    proto::MobileConnectionState mobile_status) {
  switch (mobile_status) {
    case proto::MobileConnectionState::NO_SIM:
      return PhoneStatusModel::MobileStatus::kNoSim;
    case proto::MobileConnectionState::SIM_BUT_NO_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimButNoReception;
    case proto::MobileConnectionState::SIM_WITH_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimWithReception;
    default:
      return PhoneStatusModel::MobileStatus::kNoSim;
  }
}

PhoneStatusModel::SignalStrength GetSignalStrengthFromProto(
    proto::SignalStrength signal_strength) {
  switch (signal_strength) {
    case proto::SignalStrength::ZERO_BARS:
      return PhoneStatusModel::SignalStrength::kZeroBars;
    case proto::SignalStrength::ONE_BAR:
      return PhoneStatusModel::SignalStrength::kOneBar;
    case proto::SignalStrength::TWO_BARS:
      return PhoneStatusModel::SignalStrength::kTwoBars;
    case proto::SignalStrength::THREE_BARS:
      return PhoneStatusModel::SignalStrength::kThreeBars;
    case proto::SignalStrength::FOUR_BARS:
      return PhoneStatusModel::SignalStrength::kFourBars;
    default:
      return PhoneStatusModel::SignalStrength::kZeroBars;
  }
}

PhoneStatusModel::ChargingState GetChargingStateFromProto(
    proto::ChargingState charging_state) {
  switch (charging_state) {
    case proto::ChargingState::NOT_CHARGING:
      return PhoneStatusModel::ChargingState::kNotCharging;
    case proto::ChargingState::CHARGING_AC:
    case proto::ChargingState::CHARGING_WIRELESS:
      return PhoneStatusModel::ChargingState::kChargingAc;
    case proto::ChargingState::CHARGING_USB:
      return PhoneStatusModel::ChargingState::kChargingUsb;
    default:
      return PhoneStatusModel::ChargingState::kNotCharging;
  }
}

PhoneStatusModel::BatterySaverState GetBatterySaverStateFromProto(
    proto::BatteryMode battery_mode) {
  switch (battery_mode) {
    case proto::BatteryMode::BATTERY_SAVER_OFF:
      return PhoneStatusModel::BatterySaverState::kOff;
    case proto::BatteryMode::BATTERY_SAVER_ON:
      return PhoneStatusModel::BatterySaverState::kOn;
    default:
      return PhoneStatusModel::BatterySaverState::kOff;
  }
}

MultideviceFeatureAccessManager::AccessStatus ComputeNotificationAccessState(
    const proto::PhoneProperties& phone_properties) {
  // If the user has a Work Profile active, notification access is not allowed
  // by Android. See https://crbug.com/1155151.
  if (phone_properties.profile_type() == proto::ProfileType::WORK_PROFILE)
    return MultideviceFeatureAccessManager::AccessStatus::kProhibited;

  if (phone_properties.notification_access_state() ==
      proto::NotificationAccessState::ACCESS_GRANTED) {
    return MultideviceFeatureAccessManager::AccessStatus::kAccessGranted;
  }

  return MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted;
}

// User has to consent and agree for phoneHub to have storage permission on the
// phone
MultideviceFeatureAccessManager::AccessStatus ComputeCameraRollAccessState(
    const proto::PhoneProperties& phone_properties) {
  if (phone_properties.camera_roll_access_state().feature_enabled()) {
    return MultideviceFeatureAccessManager::AccessStatus::kAccessGranted;
  } else {
    return MultideviceFeatureAccessManager::AccessStatus::
        kAvailableButNotGranted;
  }
}

MultideviceFeatureAccessManager::AccessProhibitedReason
ComputeNotificationAccessProhibitedReason(
    const proto::PhoneProperties& phone_properties) {
  if (phone_properties.profile_disable_reason() ==
      proto::ProfileDisableReason::DISABLE_REASON_DISABLED_BY_POLICY) {
    return MultideviceFeatureAccessManager::AccessProhibitedReason::
        kDisabledByPhonePolicy;
  }
  if (phone_properties.profile_type() == proto::ProfileType::WORK_PROFILE) {
    return MultideviceFeatureAccessManager::AccessProhibitedReason::
        kWorkProfile;
  }
  return MultideviceFeatureAccessManager::AccessProhibitedReason::kUnknown;
}

ScreenLockManager::LockStatus ComputeScreenLockState(
    const proto::PhoneProperties& phone_properties) {
  switch (phone_properties.screen_lock_state()) {
    case proto::ScreenLockState::SCREEN_LOCK_UNKNOWN:
      return ScreenLockManager::LockStatus::kUnknown;
    case proto::ScreenLockState::SCREEN_LOCK_OFF:
      return ScreenLockManager::LockStatus::kLockedOff;
    case proto::ScreenLockState::SCREEN_LOCK_ON:
      return ScreenLockManager::LockStatus::kLockedOn;
    default:
      return ScreenLockManager::LockStatus::kUnknown;
  }
}

FindMyDeviceController::Status ComputeFindMyDeviceStatus(
    const proto::PhoneProperties& phone_properties) {
  if (phone_properties.find_my_device_capability() ==
      proto::FindMyDeviceCapability::NOT_ALLOWED) {
    return FindMyDeviceController::Status::kRingingNotAvailable;
  }

  bool is_ringing =
      phone_properties.ring_status() == proto::FindMyDeviceRingStatus::RINGING;

  return is_ringing ? FindMyDeviceController::Status::kRingingOn
                    : FindMyDeviceController::Status::kRingingOff;
}

PhoneStatusModel CreatePhoneStatusModel(const proto::PhoneProperties& proto) {
  PA_LOG(INFO) << "Creating PhoneStatusModel from PhoneProperties message.";
  return PhoneStatusModel(
      GetMobileStatusFromProto(proto.connection_state()),
      PhoneStatusModel::MobileConnectionMetadata{
          GetSignalStrengthFromProto(proto.signal_strength()),
          base::UTF8ToUTF16(proto.mobile_provider())},
      GetChargingStateFromProto(proto.charging_state()),
      GetBatterySaverStateFromProto(proto.battery_mode()),
      proto.battery_percentage());
}

std::vector<RecentAppsInteractionHandler::UserState> GetUserStates(
    const RepeatedPtrField<proto::UserState>& user_states) {
  std::vector<RecentAppsInteractionHandler::UserState> states;

  for (const auto& user_state : user_states) {
    RecentAppsInteractionHandler::UserState state;
    state.user_id = user_state.user_id();
    state.is_enabled = !user_state.is_quiet_mode_enabled();
    states.emplace_back(state);
  }
  return states;
}

bool ShouldUpdateRecents(
    PhoneStatusProcessor::AppListUpdateType app_list_update_type) {
  return app_list_update_type ==
             PhoneStatusProcessor::AppListUpdateType::kOnlyRecentApps ||
         app_list_update_type == PhoneStatusProcessor::AppListUpdateType::kBoth;
}

bool ShouldUpdateLauncher(
    PhoneStatusProcessor::AppListUpdateType app_list_update_type) {
  return app_list_update_type ==
         PhoneStatusProcessor::AppListUpdateType::kOnlyLauncherApps;
}

bool IsIncrementalAppUpdate(
    PhoneStatusProcessor::AppListUpdateType app_list_update_type) {
  return app_list_update_type ==
         PhoneStatusProcessor::AppListUpdateType::kIncrementalAppUpdate;
}

}  // namespace

PhoneStatusProcessor::PhoneStatusProcessor(
    DoNotDisturbController* do_not_disturb_controller,
    FeatureStatusProvider* feature_status_provider,
    MessageReceiver* message_receiver,
    FindMyDeviceController* find_my_device_controller,
    MultideviceFeatureAccessManager* multidevice_feature_access_manager,
    ScreenLockManager* screen_lock_manager,
    NotificationProcessor* notification_processor_,
    MultiDeviceSetupClient* multidevice_setup_client,
    MutablePhoneModel* phone_model,
    RecentAppsInteractionHandler* recent_apps_interaction_handler,
    PrefService* pref_service,
    AppStreamManager* app_stream_manager,
    AppStreamLauncherDataModel* app_stream_launcher_data_model,
    IconDecoder* icon_decoder,
    PhoneHubUiReadinessRecorder* phone_hub_ui_readiness_recorder,
    PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger)
    : do_not_disturb_controller_(do_not_disturb_controller),
      feature_status_provider_(feature_status_provider),
      message_receiver_(message_receiver),
      find_my_device_controller_(find_my_device_controller),
      multidevice_feature_access_manager_(multidevice_feature_access_manager),
      screen_lock_manager_(screen_lock_manager),
      notification_processor_(notification_processor_),
      multidevice_setup_client_(multidevice_setup_client),
      phone_model_(phone_model),
      recent_apps_interaction_handler_(recent_apps_interaction_handler),
      pref_service_(pref_service),
      app_stream_manager_(app_stream_manager),
      app_stream_launcher_data_model_(app_stream_launcher_data_model),
      icon_decoder_(icon_decoder),
      phone_hub_ui_readiness_recorder_(phone_hub_ui_readiness_recorder),
      phone_hub_structured_metrics_logger_(
          phone_hub_structured_metrics_logger) {
  DCHECK(do_not_disturb_controller_);
  DCHECK(feature_status_provider_);
  DCHECK(message_receiver_);
  DCHECK(find_my_device_controller_);
  DCHECK(multidevice_feature_access_manager_);
  DCHECK(notification_processor_);
  DCHECK(multidevice_setup_client_);
  DCHECK(phone_model_);
  DCHECK(pref_service_);
  DCHECK(app_stream_manager_);
  DCHECK(icon_decoder_);
  DCHECK(phone_hub_ui_readiness_recorder_);
  DCHECK(phone_hub_structured_metrics_logger_);

  message_receiver_->AddObserver(this);
  feature_status_provider_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  MaybeSetPhoneModelName(multidevice_setup_client_->GetHostStatus().second);
}

PhoneStatusProcessor::~PhoneStatusProcessor() {
  message_receiver_->RemoveObserver(this);
  feature_status_provider_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void PhoneStatusProcessor::ProcessReceivedNotifications(
    const RepeatedPtrField<proto::Notification>& notification_protos) {
  multidevice_setup::mojom::FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kPhoneHubNotifications);
  if (feature_state != multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    // Do not process any notifications if notifications are not enabled in
    // settings.
    return;
  }

  std::vector<proto::Notification> inline_replyable_protos;

  for (const auto& proto : notification_protos) {
    if (!features::IsPhoneHubCallNotificationEnabled() &&
        (proto.category() == proto::Notification::Category::
                                 Notification_Category_INCOMING_CALL ||
         proto.category() == proto::Notification::Category::
                                 Notification_Category_ONGOING_CALL ||
         proto.category() == proto::Notification::Category::
                                 Notification_Category_SCREEN_CALL)) {
      continue;
    }
    inline_replyable_protos.emplace_back(proto);
  }

  notification_processor_->AddNotifications(inline_replyable_protos);
}

void PhoneStatusProcessor::SetReceivedPhoneStatusModelStates(
    const proto::PhoneProperties& phone_properties) {
  phone_hub_structured_metrics_logger_->ProcessPhoneInformation(
      phone_properties);
  phone_model_->SetPhoneStatusModel(CreatePhoneStatusModel(phone_properties));

  do_not_disturb_controller_->SetDoNotDisturbStateInternal(
      phone_properties.notification_mode() ==
          proto::NotificationMode::DO_NOT_DISTURB_ON,
      phone_properties.profile_type() != proto::ProfileType::WORK_PROFILE);

  multidevice_feature_access_manager_->SetNotificationAccessStatusInternal(
      ComputeNotificationAccessState(phone_properties),
      ComputeNotificationAccessProhibitedReason(phone_properties));

  if (features::IsPhoneHubCameraRollEnabled()) {
    multidevice_feature_access_manager_->SetCameraRollAccessStatusInternal(
        ComputeCameraRollAccessState(phone_properties));
  }

  if (screen_lock_manager_) {
    screen_lock_manager_->SetLockStatusInternal(
        ComputeScreenLockState(phone_properties));
  }

  find_my_device_controller_->SetPhoneRingingStatusInternal(
      ComputeFindMyDeviceStatus(phone_properties));

  if (features::IsEcheSWAEnabled()) {
    recent_apps_interaction_handler_->set_user_states(
        GetUserStates(phone_properties.user_states()));

    SetEcheFeatureStatusReceivedFromPhoneHub(
        phone_properties.eche_feature_status());
  }

  multidevice_feature_access_manager_->SetFeatureSetupRequestSupportedInternal(
      phone_properties.feature_setup_config()
          .feature_setup_request_supported());
}

void PhoneStatusProcessor::MaybeSetPhoneModelName(
    const std::optional<multidevice::RemoteDeviceRef>& remote_device) {
  if (!remote_device.has_value()) {
    phone_model_->SetPhoneName(std::nullopt);
    return;
  }

  phone_model_->SetPhoneName(base::UTF8ToUTF16(remote_device->name()));
}

void PhoneStatusProcessor::SetEcheFeatureStatusReceivedFromPhoneHub(
    proto::FeatureStatus eche_feature_status) {
  auto eche_support_received_from_phone_hub =
      ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSpecified;
  if (eche_feature_status == proto::FeatureStatus::FEATURE_STATUS_SUPPORTED ||
      eche_feature_status == proto::FeatureStatus::FEATURE_STATUS_ENABLED ||
      eche_feature_status ==
          proto::FeatureStatus::FEATURE_STATUS_PROHIBITED_BY_POLICY) {
    eche_support_received_from_phone_hub =
        ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kSupported;
  } else if (eche_feature_status ==
                 proto::FeatureStatus::FEATURE_STATUS_UNSUPPORTED ||
             eche_feature_status ==
                 proto::FeatureStatus::FEATURE_STATUS_ATTESTATION_FAILED) {
    eche_support_received_from_phone_hub =
        ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSupported;
  } else if (eche_feature_status ==
             proto::FeatureStatus::FEATURE_STATUS_UNSPECIFIED) {
    eche_support_received_from_phone_hub =
        ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSpecified;
  } else {
    NOTREACHED_IN_MIGRATION();
    eche_support_received_from_phone_hub =
        ash::multidevice_setup::EcheSupportReceivedFromPhoneHub::kNotSpecified;
  }

  pref_service_->SetInteger(
      ash::multidevice_setup::
          kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      static_cast<int>(eche_support_received_from_phone_hub));
}

void PhoneStatusProcessor::OnFeatureStatusChanged() {
  // Reset phone model instance when but still keep the phone's name.
  if (feature_status_provider_->GetStatus() !=
      FeatureStatus::kEnabledAndConnected) {
    phone_model_->SetPhoneStatusModel(std::nullopt);
    notification_processor_->ClearNotificationsAndPendingUpdates();
  }
}

void PhoneStatusProcessor::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  PA_LOG(INFO) << "Received snapshot from phone with Android version "
               << phone_status_snapshot.properties().android_version()
               << " and GmsCore version "
               << phone_status_snapshot.properties().gmscore_version();

  phone_hub_ui_readiness_recorder_->RecordPhoneStatusSnapShotReceived();

  if (features::IsEcheLauncherEnabled() && features::IsEcheSWAEnabled() &&
      !has_received_first_app_list_update_ &&
      connection_initialized_timestamp_ == base::TimeTicks()) {
    connection_initialized_timestamp_ = base::TimeTicks::Now();
  }

  ProcessReceivedNotifications(phone_status_snapshot.notifications());
  SetReceivedPhoneStatusModelStates(phone_status_snapshot.properties());
  if (features::IsEcheSWAEnabled()) {
    GenerateAppListWithIcons(phone_status_snapshot.streamable_apps(),
                             AppListUpdateType::kBoth);
  }
  multidevice_feature_access_manager_
      ->UpdatedFeatureSetupConnectionStatusIfNeeded();
}

void PhoneStatusProcessor::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  ProcessReceivedNotifications(phone_status_update.updated_notifications());
  SetReceivedPhoneStatusModelStates(phone_status_update.properties());

  if (!phone_status_update.removed_notification_ids().empty()) {
    base::flat_set<int64_t> removed_notification_ids;
    for (auto& id : phone_status_update.removed_notification_ids()) {
      removed_notification_ids.emplace(id);
    }

    notification_processor_->RemoveNotifications(removed_notification_ids);
  }
}

void PhoneStatusProcessor::OnAppStreamUpdateReceived(
    const proto::AppStreamUpdate app_stream_update) {
  if (!app_stream_update.has_foreground_app())
    return;
  auto* app = &app_stream_update.foreground_app();
  if (app->icon().empty())
    return;
  app_stream_manager_->NotifyAppStreamUpdate(app_stream_update);
}

void PhoneStatusProcessor::OnHostStatusChanged(
    const MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  MaybeSetPhoneModelName(host_device_with_status.second);
}

void PhoneStatusProcessor::OnAppListUpdateReceived(
    const proto::AppListUpdate app_list_update) {
  if (!features::IsEcheSWAEnabled()) {
    return;
  }
  if (app_list_update.has_all_apps() && features::IsEcheLauncherEnabled()) {
    GenerateAppListWithIcons(app_list_update.all_apps(),
                             AppListUpdateType::kOnlyLauncherApps);
  }
  if (app_list_update.has_recent_apps()) {
    GenerateAppListWithIcons(app_list_update.recent_apps(),
                             AppListUpdateType::kOnlyRecentApps);
  }
}

void PhoneStatusProcessor::OnAppListIncrementalUpdateReceived(
    const proto::AppListIncrementalUpdate app_incremental_update) {
  if (!features::IsEcheLauncherEnabled()) {
    return;
  }

  if (app_incremental_update.has_removed_apps()) {
    for (const auto& app : app_incremental_update.removed_apps().apps()) {
      if (app_stream_launcher_data_model_) {
        app_stream_launcher_data_model_->RemoveAppFromList(app);
      }
      if (recent_apps_interaction_handler_) {
        recent_apps_interaction_handler_->RemoveStreamableApp(app);
      }
    }
  }

  if (app_incremental_update.has_installed_apps()) {
    GenerateAppListWithIcons(app_incremental_update.installed_apps(),
                             AppListUpdateType::kIncrementalAppUpdate);
  }
}

void PhoneStatusProcessor::GenerateAppListWithIcons(
    const proto::StreamableApps& streamable_apps,
    AppListUpdateType app_list_update_type) {
  PA_LOG(INFO) << "Received a list of " << streamable_apps.apps_size()
               << " apps, app_list_update_type="
               << static_cast<int>(app_list_update_type);
  if (streamable_apps.apps_size() == 0) {
    return;
  }
  std::unique_ptr<std::vector<IconDecoder::DecodingData>> decoding_data_list =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  std::hash<std::string> str_hash;
  gfx::Image image =
      gfx::Image(CreateVectorIcon(kPhoneHubPhoneIcon, gfx::kGoogleGrey700));
  std::vector<Notification::AppMetadata> apps_list;
  for (const auto& app : streamable_apps.apps()) {
    // TODO(nayebi): AppMetadata is no longer limited to Notification class,
    // let's move it outside of the Notification class.s2
    apps_list.emplace_back(Notification::AppMetadata(
        base::UTF8ToUTF16(app.visible_name()), app.package_name(),
        /* color_icon= */ image,
        /* monochrome_icon_mask= */ std::nullopt,
        /* icon_color = */ std::nullopt,
        /* icon_is_monochrome = */ false, app.user_id(),
        app.app_streamability_status()));
    std::string key = app.package_name() + base::NumberToString(app.user_id());
    decoding_data_list->emplace_back(
        IconDecoder::DecodingData(str_hash(key), app.icon()));
  }

  icon_decoder_->BatchDecode(
      std::move(decoding_data_list),
      base::BindOnce(&PhoneStatusProcessor::IconsDecoded,
                     weak_ptr_factory_.GetWeakPtr(), base::OwnedRef(apps_list),
                     app_list_update_type));
}

void PhoneStatusProcessor::IconsDecoded(
    std::vector<Notification::AppMetadata>& apps_list,
    AppListUpdateType app_list_update_type,
    std::unique_ptr<std::vector<IconDecoder::DecodingData>> decode_items) {
  std::hash<std::string> str_hash;
  for (const IconDecoder::DecodingData& decoding_data : *decode_items) {
    if (decoding_data.result.IsEmpty())
      continue;
    // find the associated app metadata
    for (auto& app_metadata : apps_list) {
      std::string key = app_metadata.package_name +
                        base::NumberToString(app_metadata.user_id);
      if (decoding_data.id == str_hash(key)) {
        app_metadata.color_icon = decoding_data.result;
        continue;
      }
    }
  }
  if (recent_apps_interaction_handler_ &&
      ShouldUpdateRecents(app_list_update_type)) {
    recent_apps_interaction_handler_->SetStreamableApps(apps_list);
  }

  if (features::IsEcheLauncherEnabled() && app_stream_launcher_data_model_ &&
      ShouldUpdateLauncher(app_list_update_type)) {
    app_stream_launcher_data_model_->SetAppList(apps_list);
  }
  if (app_list_update_type == AppListUpdateType::kOnlyLauncherApps &&
      !has_received_first_app_list_update_ &&
      connection_initialized_timestamp_ != base::TimeTicks()) {
    base::UmaHistogramTimes(
        "Eche.AppListUpdate.Latency",
        base::TimeTicks::Now() - connection_initialized_timestamp_);
    has_received_first_app_list_update_ = true;
  }

  if (features::IsEcheLauncherEnabled() &&
      IsIncrementalAppUpdate(app_list_update_type)) {
    if (app_stream_launcher_data_model_) {
      app_stream_launcher_data_model_->AddAppToList(apps_list.at(0));
    }
  }
}

}  // namespace phonehub
}  // namespace ash
