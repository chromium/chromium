// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_trigger_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom.h"
#include "components/cross_device/logging/logging.h"

namespace {

// Text Payload Example used in SendText().
const char kPayloadExample[] = "TEXT PAYLOAD EXAMPLE";

// Keys in the JSON representation of a dictionaries sent to the UITriggerTab.
const char kTimeStampKey[] = "time";
const char kShareTargetDeviceNamesKey[] = "deviceName";
const char kShareTargetIdKey[] = "share_targetId";
const char kStatusCodeKey[] = "statusCode";
const char kTriggerEventKey[] = "triggerEvent";
const char kTransferUpdateMetaDataKey[] = "transfer_metadataStatus";

// Keys in the JSON representation of a dictiory send to UITriggerTab for
// the state of the transfer.
const char kIsConnecting[] = "isConnecting";
const char kIsInHighVisibility[] = "isInHighVisibility";
const char kIsReceiving[] = "isReceiving";
const char kIsScanning[] = "isScanning";
const char kIsSending[] = "isSending";
const char kIsTransferring[] = "isTransferring";

// KFields used in ShowReceiveNotification.
const char kShareTargetFakeFullName[] = "Daniel's Rotom";
const char kTextAttachmentFakeBodyText[] = "Long text that should be truncated";

// TriggerEvents in alphabetical order.
enum class TriggerEvent {
  kAccept,
  kCancel,
  kOpen,
  kRegisterBackgroundReceiveSurface,
  kRegisterBackgroundSendSurface,
  kRegisterForegroundReceiveSurface,
  kRegisterForegroundSendSurface,
  kReject,
  kSendText,
  kUnregisterReceiveSurface,
  kUnregisterSendSurface,
};

base::Value GetJavascriptTimestamp() {
  return base::Value(
      base::Time::Now().InMillisecondsFSinceUnixEpochIgnoringNull());
}

std::string StatusCodeToString(
    const NearbySharingService::StatusCodes status_code) {
  std::string status_code_string;
  switch (status_code) {
    case NearbySharingService::StatusCodes::kOk:
      return "OK";
    case NearbySharingService::StatusCodes::kError:
      return "ERROR";
    case NearbySharingService::StatusCodes::kOutOfOrderApiCall:
      return "OUT OF ORDER API CALL";
    case NearbySharingService::StatusCodes::kStatusAlreadyStopped:
      return "STATUS ALREADY STOPPED";
    case NearbySharingService::StatusCodes::kTransferAlreadyInProgress:
      return "TRANSFER ALREADY IN PROGRESS";
    case NearbySharingService::StatusCodes::kNoAvailableConnectionMedium:
      return "NO AVAILABLE CONNECTION MEDIUM";
  }
}

std::string TriggerEventToString(const TriggerEvent trigger_event) {
  switch (trigger_event) {
    case TriggerEvent::kSendText:
      return "SendText";
    case TriggerEvent::kAccept:
      return "Accept";
    case TriggerEvent::kCancel:
      return "Cancel";
    case TriggerEvent::kOpen:
      return "Open";
    case TriggerEvent::kReject:
      return "Reject";
    case TriggerEvent::kRegisterBackgroundSendSurface:
      return "Register SendSurface (BACKGROUND)";
    case TriggerEvent::kRegisterForegroundSendSurface:
      return "Register SendSurface (FOREGROUND)";
    case TriggerEvent::kUnregisterSendSurface:
      return "Unregister SendSurface";
    case TriggerEvent::kRegisterBackgroundReceiveSurface:
      return "Register ReceiveSurface (BACKGROUND)";
    case TriggerEvent::kRegisterForegroundReceiveSurface:
      return "Register ReceiveSurface (FOREGROUND)";
    case TriggerEvent::kUnregisterReceiveSurface:
      return "Unregister ReceiveSurface";
  }
}

std::string TransferUpdateMetaDataToString(
    const TransferMetadata& transfer_metadata) {
  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kUnknown:
      return "Transfer status: Unknown";
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
      return "Transfer status: Awaiting Local Confirmation";
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      return "Transfer status: Awaiting Remote Acceptance";
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return "Transfer status: Awaiting Remote Acceptance Failed";
    case TransferMetadata::Status::kInProgress:
      return "Transfer status: In Progress";
    case TransferMetadata::Status::kComplete:
      return "Transfer status: Complete";
    case TransferMetadata::Status::kFailed:
      return "Transfer status: Failed";
    case TransferMetadata::Status::kRejected:
      return "Transfer status: Rejected";
    case TransferMetadata::Status::kCancelled:
      return "Transfer status: Cancelled";
    case TransferMetadata::Status::kTimedOut:
      return "Transfer status: Timed Out";
    case TransferMetadata::Status::kMediaUnavailable:
      return "Transfer status: Media Unavailable";
    case TransferMetadata::Status::kMediaDownloading:
      return "Transfer status: Media Downloading";
    case TransferMetadata::Status::kNotEnoughSpace:
      return "Transfer status: Not Enough Space";
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return "Transfer status: Unsupported Attachment Type";
    case TransferMetadata::Status::kExternalProviderLaunched:
      return "Transfer status: External Provider Launched";
    case TransferMetadata::Status::kConnecting:
      return "Transfer status: Connecting";
    case TransferMetadata::Status::kDecodeAdvertisementFailed:
      return "Transfer status: Decode Advertistement Failed";
    case TransferMetadata::Status::kMissingTransferUpdateCallback:
      return "Transfer status: Missing Transfer Update Callback";
    case TransferMetadata::Status::kMissingShareTarget:
      return "Transfer status: Missing Share Target";
    case TransferMetadata::Status::kMissingEndpointId:
      return "Transfer status: Missing Endpoint Id";
    case TransferMetadata::Status::kMissingPayloads:
      return "Transfer status: Missing Payloads";
    case TransferMetadata::Status::kPairedKeyVerificationFailed:
      return "Transfer status: Paired Key Verification Failed";
    case TransferMetadata::Status::kInvalidIntroductionFrame:
      return "Transfer status: Invalid Introduction Frame";
    case TransferMetadata::Status::kIncompletePayloads:
      return "Transfer status: Incomplete Payloads";
    case TransferMetadata::Status::kFailedToCreateShareTarget:
      return "Transfer status: Failed To Create Share Target";
    case TransferMetadata::Status::kFailedToInitiateOutgoingConnection:
      return "Transfer status: Failed To Initiate Outgoing Connection";
    case TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse:
      return "Transfer status: Failed To Read Outgoing Connection Response.";
    case TransferMetadata::Status::kUnexpectedDisconnection:
      return "Transfer status: Unexpected Disconnection";
  }
}

// Converts |status_code| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value::Dict StatusCodeToDictionary(
    const NearbySharingService::StatusCodes status_code,
    TriggerEvent trigger_event) {
  base::Value::Dict dictionary;
  dictionary.Set(kStatusCodeKey, StatusCodeToString(status_code));
  dictionary.Set(kTriggerEventKey, TriggerEventToString(trigger_event));
  dictionary.Set(kTimeStampKey, GetJavascriptTimestamp());
  return dictionary;
}

// Converts |share_target| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value::Dict ShareTargetToDictionary(const ShareTarget share_target) {
  base::Value::Dict share_target_dictionary;
  share_target_dictionary.Set(kShareTargetDeviceNamesKey,
                              share_target.device_name);
  share_target_dictionary.Set(kShareTargetIdKey, share_target.id.ToString());
  share_target_dictionary.Set(kTimeStampKey, GetJavascriptTimestamp());
  return share_target_dictionary;
}

// Converts |id_to_share_target_map| to a raw dictionary value used as a JSON
// argument to JavaScript functions.
base::Value::List ShareTargetMapToList(
    const base::flat_map<std::string, ShareTarget>& id_to_share_target_map) {
  base::Value::List share_target_list;
  share_target_list.reserve(id_to_share_target_map.size());

  for (const auto& it : id_to_share_target_map) {
    share_target_list.Append(ShareTargetToDictionary(it.second));
  }

  return share_target_list;
}

// Converts |transfer_metadata| to a raw dictionary value used as a JSON
// argument to JavaScript functions.
base::Value::Dict TransferUpdateToDictionary(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  base::Value::Dict dictionary;
  dictionary.Set(kTransferUpdateMetaDataKey,
                 TransferUpdateMetaDataToString(transfer_metadata));
  dictionary.Set(kTimeStampKey, GetJavascriptTimestamp());
  dictionary.Set(kShareTargetDeviceNamesKey, share_target.device_name);
  dictionary.Set(kShareTargetIdKey, share_target.id.ToString());
  return dictionary;
}

base::Value::Dict StatusBooleansToDictionary(const bool is_scanning,
                                             const bool is_transferring,
                                             const bool is_receiving_files,
                                             const bool is_sending_files,
                                             const bool is_conecting,
                                             const bool is_in_high_visibility) {
  base::Value::Dict dictionary;
  dictionary.Set(kIsScanning, is_scanning);
  dictionary.Set(kIsTransferring, is_transferring);
  dictionary.Set(kIsSending, is_sending_files);
  dictionary.Set(kIsReceiving, is_receiving_files);
  dictionary.Set(kIsConnecting, is_conecting);
  dictionary.Set(kIsInHighVisibility, is_in_high_visibility);
  dictionary.Set(kTimeStampKey, GetJavascriptTimestamp());
  return dictionary;
}

}  // namespace

NearbyInternalsUiTriggerHandler::NearbyInternalsUiTriggerHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsUiTriggerHandler::~NearbyInternalsUiTriggerHandler() = default;

void NearbyInternalsUiTriggerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeUiTrigger",
      base::BindRepeating(&NearbyInternalsUiTriggerHandler::InitializeContents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "sendText",
      base::BindRepeating(&NearbyInternalsUiTriggerHandler::SendText,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "accept", base::BindRepeating(&NearbyInternalsUiTriggerHandler::Accept,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reject", base::BindRepeating(&NearbyInternalsUiTriggerHandler::Reject,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel", base::BindRepeating(&NearbyInternalsUiTriggerHandler::Cancel,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "open", base::BindRepeating(&NearbyInternalsUiTriggerHandler::Open,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerSendSurfaceForeground",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::RegisterSendSurfaceForeground,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerSendSurfaceBackground",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::RegisterSendSurfaceBackground,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "unregisterSendSurface",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::UnregisterSendSurface,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerReceiveSurfaceForeground",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceForeground,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerReceiveSurfaceBackground",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceBackground,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "unregisterReceiveSurface",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::UnregisterReceiveSurface,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStates",
      base::BindRepeating(&NearbyInternalsUiTriggerHandler::GetState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showNearbyShareReceivedNotification",
      base::BindRepeating(
          &NearbyInternalsUiTriggerHandler::ShowReceivedNotification,
          base::Unretained(this)));
}

void NearbyInternalsUiTriggerHandler::InitializeContents(
    const base::Value::List& args) {
  AllowJavascript();
}

void NearbyInternalsUiTriggerHandler::RegisterSendSurfaceForeground(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterSendSurface(
              this, this, NearbySharingService::SendSurfaceState::kForeground),
          TriggerEvent::kRegisterForegroundSendSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterSendSurfaceBackground(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterSendSurface(
              this, this, NearbySharingService::SendSurfaceState::kBackground),
          TriggerEvent::kRegisterBackgroundSendSurface));
}

void NearbyInternalsUiTriggerHandler::UnregisterSendSurface(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(service_->UnregisterSendSurface(this, this),
                             TriggerEvent::kUnregisterSendSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceForeground(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterReceiveSurface(
              this, NearbySharingService::ReceiveSurfaceState::kForeground),
          TriggerEvent::kRegisterForegroundReceiveSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceBackground(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterReceiveSurface(
              this, NearbySharingService::ReceiveSurfaceState::kBackground),
          TriggerEvent::kRegisterBackgroundReceiveSurface));
}

void NearbyInternalsUiTriggerHandler::UnregisterReceiveSurface(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(service_->UnregisterReceiveSurface(this),
                             TriggerEvent::kUnregisterReceiveSurface));
}

void NearbyInternalsUiTriggerHandler::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  FireWebUIListener("transfer-updated", TransferUpdateToDictionary(
                                            share_target, transfer_metadata));
}

void NearbyInternalsUiTriggerHandler::OnShareTargetDiscovered(
    ShareTarget share_target) {
  id_to_share_target_map_.insert({share_target.id.ToString(), share_target});
  FireWebUIListener("share-target-discovered",
                    ShareTargetToDictionary(share_target));
  FireWebUIListener("share-target-map-updated",
                    ShareTargetMapToList(id_to_share_target_map_));
}

void NearbyInternalsUiTriggerHandler::OnShareTargetLost(
    ShareTarget share_target) {
  id_to_share_target_map_.erase(share_target.id.ToString());
  FireWebUIListener("share-target-lost", ShareTargetToDictionary(share_target));
  FireWebUIListener("share-target-map-updated",
                    ShareTargetMapToList(id_to_share_target_map_));
}

void NearbyInternalsUiTriggerHandler::OnAcceptCalled(
    NearbySharingService::StatusCodes status_codes) {
  FireWebUIListener(
      "on-status-code-returned",
      StatusCodeToDictionary(status_codes, TriggerEvent::kAccept));
}

void NearbyInternalsUiTriggerHandler::OnOpenCalled(
    NearbySharingService::StatusCodes status_codes) {
  FireWebUIListener("on-status-code-returned",
                    StatusCodeToDictionary(status_codes, TriggerEvent::kOpen));
}

void NearbyInternalsUiTriggerHandler::OnRejectCalled(
    NearbySharingService::StatusCodes status_codes) {
  FireWebUIListener(
      "on-status-code-returned",
      StatusCodeToDictionary(status_codes, TriggerEvent::kReject));
}

void NearbyInternalsUiTriggerHandler::OnCancelCalled(
    NearbySharingService::StatusCodes status_codes) {
  FireWebUIListener(
      "on-status-code-returned",
      StatusCodeToDictionary(status_codes, TriggerEvent::kCancel));
}

void NearbyInternalsUiTriggerHandler::SendText(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args[1].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Invalid ShareTarget ID " << share_target_id << " for SendText.";
    return;
  }

  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.push_back(std::make_unique<TextAttachment>(
      TextAttachment::Type::kText, kPayloadExample, /*title=*/std::nullopt,
      /*mime_type=*/std::nullopt));

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->SendAttachments(it->second, std::move(attachments)),
          TriggerEvent::kSendText));
}

void NearbyInternalsUiTriggerHandler::Accept(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Invalid ShareTarget ID " << share_target_id << " for Accept.";
    return;
  }

  service_->Accept(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnAcceptCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Open(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Invalid ShareTarget ID " << share_target_id << " for Open.";
    return;
  }

  service_->Open(it->second,
                 base::BindOnce(&NearbyInternalsUiTriggerHandler::OnOpenCalled,
                                weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Reject(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Invalid ShareTarget ID " << share_target_id << " for Reject.";
    return;
  }

  service_->Reject(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnRejectCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Cancel(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Invalid ShareTarget ID " << share_target_id << " for Cancel.";
    return;
  }

  service_->Cancel(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnCancelCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::GetState(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusBooleansToDictionary(
          service_->IsScanning(), service_->IsTransferring(),
          service_->IsReceivingFile(), service_->IsSendingFile(),
          service_->IsConnecting(), service_->IsInHighVisibility()));
}

void NearbyInternalsUiTriggerHandler::ShowReceivedNotification(
    const base::Value::List& args) {
  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service) {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
    return;
  }

  NearbyNotificationManager* manager = service->GetNotificationManager();

  if (!manager) {
    CD_LOG(ERROR, Feature::NS)
        << "No NearbyNotificationManager instance to call.";
    return;
  }

  // Create a share target with a fake text attachment.
  TextAttachment attachment(TextAttachment::Type::kText,
                            kTextAttachmentFakeBodyText,
                            /*title=*/std::nullopt,
                            /*mime_type=*/std::nullopt);
  ShareTarget target;
  target.is_incoming = true;
  target.device_name = kShareTargetFakeFullName;
  attachment.MoveToShareTarget(target);

  manager->ShowSuccess(target);
}
