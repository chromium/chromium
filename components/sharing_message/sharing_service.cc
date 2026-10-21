// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_service.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingMessageSender> message_sender,
    std::unique_ptr<SharingDeviceSource> device_source,
    std::unique_ptr<SharingHandlerRegistry> handler_registry,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    syncer::SyncService* sync_service,
    send_tab_to_self::SendTabToSelfModel* send_tab_model,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : sync_prefs_(std::move(sync_prefs)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      message_sender_(std::move(message_sender)),
      device_source_(std::move(device_source)),
      handler_registry_(std::move(handler_registry)),
      fcm_handler_(std::move(fcm_handler)),
      sync_service_(sync_service),
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
    UnregisterDevice();
  }

  // `send_tab_model_` can be null in tests.
  if (send_tab_model) {
    send_tab_to_self_scoped_observation_.Observe(send_tab_model);
  }
}

SharingService::~SharingService() {
  // `sync_service_` should be reset in `Shutdown` method.
  DCHECK(!sync_service_);
}

std::optional<SharingTargetDeviceInfo> SharingService::GetDeviceByGuid(
    const std::string& guid) const {
  return device_source_->GetDeviceByGuid(guid);
}

SharingService::SharingDeviceList SharingService::GetDeviceCandidates(
    syncer::DeviceInfo::SharingFeature required_feature) const {
  return device_source_->GetDeviceCandidates(required_feature);
}

base::OnceClosure SharingService::SendMessageToDevice(
    const SharingTargetDeviceInfo& device,
    base::TimeDelta response_timeout,
    components_sharing_message::SharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  return message_sender_->SendMessageToDevice(
      device, response_timeout, std::move(message), std::move(callback));
}

base::OnceClosure SharingService::SendIosPushMessageToDevice(
    const SharingTargetDeviceInfo& device,
    sync_pb::UnencryptedSharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  return message_sender_->SendIosPushMessageToDevice(device, std::move(message),
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

void SharingService::OnEntryAddedLocally(
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  std::optional<SharingTargetDeviceInfo> target_device_info =
      GetDeviceByGuid(entry->GetTargetDeviceSyncCacheGuid());

  if (!target_device_info.has_value()) {
    return;
  }

  if (target_device_info.value().platform() != SharingDevicePlatform::kIOS) {
    return;
  }
  SendNotificationForSendTabToSelfPush(
      send_tab_to_self::SendTabToSelfEntry(*entry));
}

void SharingService::ResetConnectionToSyncService() {
  if (sync_service_ && sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
  sync_service_ = nullptr;
}

void SharingService::Shutdown() {
  // Avoid dangling `raw_ptr`s by explicitly resetting/destroying early fields
  // and objects that maintain `raw_ptr`s to things owned by other services.
  ResetConnectionToSyncService();
  sharing_device_registration_.reset();
  send_tab_to_self_scoped_observation_.Reset();
}

void SharingService::OnSyncShutdown(syncer::SyncService* sync) {
  ResetConnectionToSyncService();
}

void SharingService::OnStateChanged(syncer::SyncService* sync) {
  if (IsSyncEnabledForSharing(sync_service_) && state_ == State::DISABLED) {
    RegisterDevice();
  } else if (IsSyncDisabledForSharing(sync_service_) &&
             (state_ == State::ACTIVE || state_ == State::REGISTERING)) {
    // Unregister both when already `ACTIVE` and when still `REGISTERING` (e.g.
    // if registration is in flight or waiting for a backoff retry when sync is
    // disabled).
    UnregisterDevice();
  }
}

void SharingService::RegisterDevice() {
  // TODO(crbug.com/574368479): Guarantee at most one in-flight registration
  // (e.g. across rapid Register -> Unregister -> Register transitions).
  state_ = State::REGISTERING;
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::RetryRegisterDevice() {
  // If state has changed while waiting for a backoff retry, ignore.
  if (state_ != State::REGISTERING) {
    return;
  }
  CHECK(!IsSyncDisabledForSharing(sync_service_), base::NotFatalUntil::M161);

  RegisterDevice();
}

void SharingService::RegisterDeviceInTesting(
    std::set<syncer::DeviceInfo::SharingFeature> enabled_features,
    SharingDeviceRegistration::RegistrationCallback callback) {
  sharing_device_registration_->SetEnabledFeaturesForTesting(
      std::move(enabled_features));
  sharing_device_registration_->RegisterDevice(std::move(callback));
}

void SharingService::UnregisterDevice() {
  // Track that unregistration is in progress so `OnStateChanged()` does not
  // start a new registration before `OnDeviceUnregistered()` completes and
  // transitions the state to `REGISTERING` or `DISABLED`.
  state_ = State::UNREGISTERING;
  sharing_device_registration_->UnregisterDevice(base::BindOnce(
      &SharingService::OnDeviceUnregistered, weak_ptr_factory_.GetWeakPtr()));
  message_sender_->ClearPendingMessages();
}

void SharingService::OnDeviceRegistered(
    SharingDeviceRegistrationResult result) {
  if (state_ != State::REGISTERING) {
    return;
  }
  if (IsSyncDisabledForSharing(sync_service_)) {
    // In case sync is disabled during registration, unregister it.
    UnregisterDevice();
    return;
  }

  backoff_entry_.InformOfRequest(result ==
                                 SharingDeviceRegistrationResult::kSuccess);

  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      state_ = State::ACTIVE;
      fcm_handler_->StartListening();
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      // Transient error - try again after a delay.
      LOG(ERROR) << "Device registration failed with transient error";
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SharingService::RetryRegisterDevice,
                         weak_ptr_factory_.GetWeakPtr()),
          backoff_entry_.GetTimeUntilRelease());
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
    case SharingDeviceRegistrationResult::kInternalError:
      // No need to bother retrying in the case of one of fatal errors.
      LOG(ERROR) << "Device registration failed with fatal error";
      state_ = State::DISABLED;
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Register device cannot return kDeviceNotRegistered.
      NOTREACHED();
  }
}

void SharingService::OnDeviceUnregistered(
    SharingDeviceRegistrationResult result) {
  if (IsSyncEnabledForSharing(sync_service_)) {
    // In case sync is enabled during un-registration, register it.
    RegisterDevice();
  } else {
    // Stop listening only after unregistration finishes deleting the FCM
    // token. `StopListening()` removes the `GCMAppHandler` from `GCMDriver`,
    // which would otherwise cause `GCMDriver::DeleteToken()` to fail if no app
    // handlers remain.
    fcm_handler_->StopListening();
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
    const send_tab_to_self::SendTabToSelfEntry& entry) {
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
      base::UTF8ToUTF16(entry.GetURL().GetHost()));

  push_notification_entry->set_title(title);
  push_notification_entry->set_text(body);
  push_notification_entry->set_destination_url(entry.GetURL().spec());
  push_notification_entry->set_placeholder_title(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_TITLE));
  push_notification_entry->set_placeholder_body(l10n_util::GetStringUTF8(
      IDS_SEND_TAB_PUSH_NOTIFICATION_PLACEHOLDER_BODY));
  push_notification_entry->set_entry_unique_guid(entry.GetGUID());

  SendIosPushMessageToDevice(target_device_info.value(),
                             std::move(sharing_message),
                             /*callback=*/base::DoNothing());
}
