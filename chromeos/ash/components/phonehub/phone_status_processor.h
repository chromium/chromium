// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_

#include <google/protobuf/repeated_field.h>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefService;

namespace ash::phonehub {

using ::google::protobuf::RepeatedPtrField;

class DoNotDisturbController;
class FindMyDeviceController;
class MutablePhoneModel;
class MultideviceFeatureAccessManager;
class NotificationProcessor;
class ScreenLockManager;
class RecentAppsInteractionHandler;
class AppStreamManager;
class PhoneHubUiReadinessRecorder;

// Responsible for receiving incoming protos and calling on clients to update
// their models.
class PhoneStatusProcessor
    : public MessageReceiver::Observer,
      public FeatureStatusProvider::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  enum class AppListUpdateType {
    kOnlyRecentApps = 0,
    kOnlyLauncherApps,
    kBoth,
    kIncrementalAppUpdate,
    kMaxValue = kIncrementalAppUpdate
  };

  PhoneStatusProcessor(
      DoNotDisturbController* do_not_disturb_controller,
      FeatureStatusProvider* feature_status_provider,
      MessageReceiver* message_receiver,
      FindMyDeviceController* find_my_device_controller,
      MultideviceFeatureAccessManager* multidevice_feature_access_manager,
      ScreenLockManager* screen_lock_manager,
      NotificationProcessor* notification_processor_,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MutablePhoneModel* phone_model,
      RecentAppsInteractionHandler* recent_apps_interaction_handler,
      PrefService* pref_service,
      AppStreamManager* app_stream_manager,
      AppStreamLauncherDataModel* app_stream_launcher_data_model,
      IconDecoder* icon_decoder_,
      PhoneHubUiReadinessRecorder* phone_hub_ui_readiness_recorder,
      PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger);
  ~PhoneStatusProcessor() override;

  PhoneStatusProcessor(const PhoneStatusProcessor&) = delete;
  PhoneStatusProcessor& operator=(const PhoneStatusProcessor&) = delete;

 private:
  friend class PhoneStatusProcessorTest;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnAppStreamUpdateReceived(
      const proto::AppStreamUpdate app_stream_update) override;
  void OnAppListUpdateReceived(
      const proto::AppListUpdate app_list_update) override;
  void OnAppListIncrementalUpdateReceived(
      const proto::AppListIncrementalUpdate app_list_incremental_update)
      override;

  // MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  void ProcessReceivedNotifications(
      const RepeatedPtrField<proto::Notification>& notification_protos);

  void SetReceivedPhoneStatusModelStates(
      const proto::PhoneProperties& phone_properties);

  void MaybeSetPhoneModelName(
      const std::optional<multidevice::RemoteDeviceRef>& remote_device);

  void SetEcheFeatureStatusReceivedFromPhoneHub(
      proto::FeatureStatus eche_feature_status);

  void GenerateAppListWithIcons(const proto::StreamableApps& streamable_apps,
                                AppListUpdateType app_list_update_type);

  void IconsDecoded(
      std::vector<Notification::AppMetadata>& apps_list,
      AppListUpdateType app_list_update_type,
      std::unique_ptr<std::vector<IconDecoder::DecodingData>> decode_items);

  raw_ptr<DoNotDisturbController> do_not_disturb_controller_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<MessageReceiver> message_receiver_;
  raw_ptr<FindMyDeviceController> find_my_device_controller_;
  raw_ptr<MultideviceFeatureAccessManager> multidevice_feature_access_manager_;
  raw_ptr<ScreenLockManager> screen_lock_manager_;
  raw_ptr<NotificationProcessor> notification_processor_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<MutablePhoneModel> phone_model_;
  raw_ptr<RecentAppsInteractionHandler> recent_apps_interaction_handler_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<AppStreamManager> app_stream_manager_;
  raw_ptr<AppStreamLauncherDataModel> app_stream_launcher_data_model_;
  raw_ptr<IconDecoder> icon_decoder_;
  raw_ptr<PhoneHubUiReadinessRecorder> phone_hub_ui_readiness_recorder_;
  raw_ptr<PhoneHubStructuredMetricsLogger> phone_hub_structured_metrics_logger_;
  base::TimeTicks connection_initialized_timestamp_ = base::TimeTicks();
  bool has_received_first_app_list_update_ = false;

  base::WeakPtrFactory<PhoneStatusProcessor> weak_ptr_factory_{this};
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_
