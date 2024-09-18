// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_service.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_device_registration_result.h"
#include "components/sharing_message/sharing_device_source.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sharing_message/vapid_key_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
constexpr int kMinimumFaviconSize = 32;
}  // namespace

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingMessageSender> message_sender,
    std::unique_ptr<SharingDeviceSource> device_source,
    std::unique_ptr<SharingHandlerRegistry> handler_registry,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    syncer::SyncService* sync_service,
    favicon::FaviconService* favicon_service,
    send_tab_to_self::SendTabToSelfModel* send_tab_model,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      message_sender_(std::move(message_sender)),
      device_source_(std::move(device_source)),
      handler_registry_(std::move(handler_registry)),
      fcm_handler_(std::move(fcm_handler)),
      sync_service_(sync_service),
      favicon_service_(favicon_service),
      task_runner_(std::move(task_runner)),
      backoff_entry_(&kRetryBackoffPolicy),
      state_(State::DISABLED) {
  // If device has already registered before, start listening to FCM right away
  // to avoid missing messages.
  if (sync_prefs_ && sync_prefs_->GetFCMRegistration()) {
    fcm_handler_->StartListening();
  }

  if (sync_service_ && !sync_service_->HasObserver(this)) {
    sync_service_->AddObserver(this);
  }

  // Only unregister if sync is disabled (not initializing).
  if (IsSyncDisabledForSharing(sync_service_)) {
    // state_ is kept as State::DISABLED as SharingService has never registered,
    // and only doing clean up via UnregisterDevice().
    UnregisterDevice();
  }

  // `send_tab_model_` can be null in tests.
  if (send_tab_model &&
      base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    send_tab_to_self_scoped_observation_.Observe(send_tab_model);
  }
}

SharingService::~SharingService() {
  if (sync_service_ && sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
}

std::optional<SharingTargetDeviceInfo> SharingService::GetDeviceByGuid(
    const std::string& guid) const {
  return device_source_->GetDeviceByGuid(guid);
}

SharingService::SharingDeviceList SharingService::GetDeviceCandidates(
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  return device_source_->GetDeviceCandidates(required_feature);
}

base::OnceClosure SharingService::SendMessageToDevice(
    const SharingTargetDeviceInfo& device,
    base::TimeDelta response_timeout,
    components_sharing_message::SharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  return message_sender_->SendMessageToDevice(
      device, response_timeout, std::move(message),
      SharingMessageSender::DelegateType::kFCM, std::move(callback));
}

base::OnceClosure SharingService::SendUnencryptedMessageToDevice(
    const SharingTargetDeviceInfo& device,
    sync_pb::UnencryptedSharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  return message_sender_->SendUnencryptedMessageToDevice(
      device, std::move(message), SharingMessageSender::DelegateType::kIOSPush,
      std::move(callback));
}

void SharingService::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  handler_registry_->RegisterSharingHandler(std::move(handler), payload_case);
}

void SharingService::UnregisterSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  handler_registry_->UnregisterSharingHandler(payload_case);
}

void SharingService::SetNotificationActionHandler(
    const std::string& notification_id,
    NotificationActionCallback callback) {
  if (callback) {
    notification_action_handlers_[notification_id] = callback;
  } else {
    notification_action_handlers_.erase(notification_id);
  }
}

SharingService::NotificationActionCallback
SharingService::GetNotificationActionHandler(
    const std::string& notification_id) const {
  auto iter = notification_action_handlers_.find(notification_id);
  return iter == notification_action_handlers_.end()
             ? NotificationActionCallback()
             : iter->second;
}

SharingDeviceSource* SharingService::GetDeviceSource() const {
  return device_source_.get();
}

SharingService::State SharingService::GetStateForTesting() const {
  return state_;
}

SharingSyncPreference* SharingService::GetSyncPreferencesForTesting() const {
  return sync_prefs_.get();
}

SharingFCMHandler* SharingService::GetFCMHandlerForTesting() const {
  return fcm_handler_.get();
}

SharingMessageSender* SharingService::GetMessageSenderForTesting() const {
  return message_sender_.get();
}

SharingMessageHandler* SharingService::GetSharingHandlerForTesting(
    components_sharing_message::SharingMessage::PayloadCase payload_case)
    const {
  return handler_registry_->GetSharingHandler(payload_case);
}

void SharingService::EntryAddedLocally(
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    return;
  }

  std::optional<SharingTargetDeviceInfo> target_device_info =
      GetDeviceByGuid(entry->GetTargetDeviceSyncCacheGuid());

  if (!target_device_info.has_value()) {
    return;
  }

  if (target_device_info.value().platform() != SharingDevicePlatform::kIOS) {
    return;
  }

  auto large_icon_types = std::vector<favicon_base::IconTypeSet>(
      {{favicon_base::IconType::kWebManifestIcon},
       {favicon_base::IconType::kFavicon},
       {favicon_base::IconType::kTouchIcon},
       {favicon_base::IconType::kTouchPrecomposedIcon}});

  // Retrieve favicon to issue notification.
  favicon_service_->GetLargestRawFaviconForPageURL(
      entry->GetURL(), large_icon_types, kMinimumFaviconSize,
      base::BindOnce(&SharingService::SendNotificationForSendTabToSelfPush,
                     weak_ptr_factory_.GetWeakPtr(),
                     send_tab_to_self::SendTabToSelfEntry(*entry)),
      &task_tracker_);
}

void SharingService::OnSyncShutdown(syncer::SyncService* sync) {
  if (sync_service_ && sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
  sync_service_ = nullptr;
}

void SharingService::OnStateChanged(syncer::SyncService* sync) {
  if (IsSyncEnabledForSharing(sync_service_) && state_ == State::DISABLED) {
    state_ = State::REGISTERING;
    RegisterDevice();
  } else if (IsSyncDisabledForSharing(sync_service_) &&
             state_ == State::ACTIVE) {
    state_ = State::UNREGISTERING;
    fcm_handler_->StopListening();
    sync_prefs_->ClearVapidKeyChangeObserver();
    UnregisterDevice();
  }
}

void SharingService::RefreshVapidKey() {
  if (vapid_key_manager_ && vapid_key_manager_->RefreshCachedKey()) {
    RegisterDevice();
  }
}

void SharingService::RegisterDevice() {
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::RegisterDeviceInTesting(
    std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
    SharingDeviceRegistration::RegistrationCallback callback) {
  sharing_device_registration_->SetEnabledFeaturesForTesting(
      std::move(enabled_features));
  sharing_device_registration_->RegisterDevice(std::move(callback));
}

void SharingService::UnregisterDevice() {
  sharing_device_registration_->UnregisterDevice(base::BindOnce(
      &SharingService::OnDeviceUnregistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::OnDeviceRegistered(
    SharingDeviceRegistrationResult result) {
  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      backoff_entry_.InformOfRequest(true);
      if (state_ == State::REGISTERING) {
        if (IsSyncEnabledForSharing(sync_service_)) {
          state_ = State::ACTIVE;
          fcm_handler_->StartListening();
          // Listen for further VAPID key changes for re-registration.
          // state_ is kept as State::ACTIVE during re-registration.
          sync_prefs_->SetVapidKeyChangeObserver(
              base::BindRepeating(&SharingService::RefreshVapidKey,
                                  weak_ptr_factory_.GetWeakPtr()));
        } else if (IsSyncDisabledForSharing(sync_service_)) {
          // In case sync is disabled during registration, unregister it.
          state_ = State::UNREGISTERING;
          UnregisterDevice();
        }
      }
      // For registration as result of VAPID key change, state_ will be
      // State::ACTIVE, and we don't need to start listeners.
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      backoff_entry_.InformOfRequest(false);
      // Transient error - try again after a delay.
      LOG(ERROR) << "Device registration failed with transient error";
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SharingService::RegisterDevice,
                         weak_ptr_factory_.GetWeakPtr()),
          backoff_entry_.GetTimeUntilRelease());
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
    case SharingDeviceRegistrationResult::kInternalError:
      backoff_entry_.InformOfRequest(false);
      // No need to bother retrying in the case of one of fatal errors.
      LOG(ERROR) << "Device registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Register device cannot return kDeviceNotRegistered.
      NOTREACHED_IN_MIGRATION();
  }
}

void SharingService::OnDeviceUnregistered(
    SharingDeviceRegistrationResult result) {
  if (IsSyncEnabledForSharing(sync_service_)) {
    // In case sync is enabled during un-registration, register it.
    state_ = State::REGISTERING;
    RegisterDevice();
  } else {
    state_ = State::DISABLED;
  }

  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      // Successfully unregistered, no-op
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      LOG(ERROR) << "Device un-registration failed with transient error";
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
    case SharingDeviceRegistrationResult::kInternalError:
      LOG(ERROR) << "Device un-registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Device has not been registered, no-op.
      break;
  }
}

void SharingService::SendNotificationForSendTabToSelfPush(
    const send_tab_to_self::SendTabToSelfEntry& entry,
    const favicon_base::FaviconRawBitmapResult& result) {
  std::optional<SharingTargetDeviceInfo> target_device_info =
      GetDeviceByGuid(entry.GetTargetDeviceSyncCacheGuid());

  sync_pb::UnencryptedSharingMessage sharing_message;
  sync_pb::SendTabToSelfPush* push_notification_entry =
      sharing_message.mutable_send_tab_message();

  std::string title = l10n_util::GetStringFUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_TITLE_USER_GIVEN_DEVICE_NAME,
      base::UTF8ToUTF16(entry.GetDeviceName()));
  std::string body = l10n_util::GetStringFUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_BODY, base::UTF8ToUTF16(entry.GetTitle()),
      base::UTF8ToUTF16(entry.GetURL().host()));

  push_notification_entry->set_title(title);
  push_notification_entry->set_text(body);
  push_notification_entry->set_destination_url(entry.GetURL().spec());
  push_notification_entry->set_placeholder_title(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_TITLE));
  push_notification_entry->set_placeholder_body(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_BODY));
  push_notification_entry->set_entry_unique_guid(entry.GetGUID());

  auto* icon = push_notification_entry->add_icon();
  icon->set_url(result.icon_url.spec());

  SendUnencryptedMessageToDevice(target_device_info.value(),
                                 std::move(sharing_message),
                                 /*callback=*/base::DoNothing());
}
