// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_trigger_handler.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chrome/services/sharing/public/mojom/nearby_share_target_types.mojom.h"

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
  return base::Value(base::Time::Now().ToJsTimeIgnoringNull());
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
      return "'Transfer status: External Provider Launched";
    case TransferMetadata::Status::kConnecting:
      return "'Transfer status: Connecting";
  }
}

// Converts |status_code| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value StatusCodeToDictionary(
    const NearbySharingService::StatusCodes status_code,
    TriggerEvent trigger_event) {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetStringKey(kStatusCodeKey, StatusCodeToString(status_code));
  dictionary.SetStringKey(kTriggerEventKey,
                          TriggerEventToString(trigger_event));
  dictionary.SetKey(kTimeStampKey, GetJavascriptTimestamp());
  return dictionary;
}

// Converts |share_target| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value ShareTargetToDictionary(const ShareTarget share_target) {
  base::Value share_target_dictionary(base::Value::Type::DICTIONARY);
  share_target_dictionary.SetStringKey(kShareTargetDeviceNamesKey,
                                       share_target.device_name);
  share_target_dictionary.SetStringKey(kShareTargetIdKey,
                                       share_target.id.ToString());
  share_target_dictionary.SetKey(kTimeStampKey, GetJavascriptTimestamp());
  return share_target_dictionary;
}

// Converts |id_to_share_target_map| to a raw dictionary value used as a JSON
// argument to JavaScript functions.
base::Value ShareTargetMapToList(
    const base::flat_map<std::string, ShareTarget>& id_to_share_target_map) {
  base::Value::ListStorage share_target_list;
  share_target_list.reserve(id_to_share_target_map.size());

  for (const auto& it : id_to_share_target_map) {
    share_target_list.push_back(ShareTargetToDictionary(it.second));
  }

  return base::Value(share_target_list);
}

// Converts |transfer_metadata| to a raw dictionary value used as a JSON
// argument to JavaScript functions.
base::Value TransferUpdateToDictionary(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetStringKey(kTransferUpdateMetaDataKey,
                          TransferUpdateMetaDataToString(transfer_metadata));
  dictionary.SetKey(kTimeStampKey, GetJavascriptTimestamp());
  dictionary.SetStringKey(kShareTargetDeviceNamesKey, share_target.device_name);
  dictionary.SetStringKey(kShareTargetIdKey, share_target.id.ToString());
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
}

void NearbyInternalsUiTriggerHandler::InitializeContents(
    const base::ListValue* args) {
  AllowJavascript();
}

void NearbyInternalsUiTriggerHandler::RegisterSendSurfaceForeground(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterSendSurface(
              this, this, NearbySharingService::SendSurfaceState::kForeground),
          TriggerEvent::kRegisterForegroundSendSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterSendSurfaceBackground(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterSendSurface(
              this, this, NearbySharingService::SendSurfaceState::kBackground),
          TriggerEvent::kRegisterBackgroundSendSurface));
}

void NearbyInternalsUiTriggerHandler::UnregisterSendSurface(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(service_->UnregisterSendSurface(this, this),
                             TriggerEvent::kUnregisterSendSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceForeground(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterReceiveSurface(
              this, NearbySharingService::ReceiveSurfaceState::kForeground),
          TriggerEvent::kRegisterForegroundReceiveSurface));
}

void NearbyInternalsUiTriggerHandler::RegisterReceiveSurfaceBackground(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->RegisterReceiveSurface(
              this, NearbySharingService::ReceiveSurfaceState::kBackground),
          TriggerEvent::kRegisterBackgroundReceiveSurface));
}

void NearbyInternalsUiTriggerHandler::UnregisterReceiveSurface(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  const base::Value& callback_id = args->GetList()[0];
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

void NearbyInternalsUiTriggerHandler::SendText(const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args->GetList()[1].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    NS_LOG(ERROR) << "Invalid ShareTarget ID " << share_target_id
                  << " for SendText.";
    return;
  }

  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.push_back(std::make_unique<TextAttachment>(
      TextAttachment::Type::kText, kPayloadExample));

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(
      callback_id,
      StatusCodeToDictionary(
          service_->SendAttachments(it->second, std::move(attachments)),
          TriggerEvent::kSendText));
}

void NearbyInternalsUiTriggerHandler::Accept(const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args->GetList()[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    NS_LOG(ERROR) << "Invalid ShareTarget ID " << share_target_id
                  << " for Accept.";
    return;
  }

  service_->Accept(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnAcceptCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Open(const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args->GetList()[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    NS_LOG(ERROR) << "Invalid ShareTarget ID " << share_target_id
                  << " for Open.";
    return;
  }

  service_->Open(it->second,
                 base::BindOnce(&NearbyInternalsUiTriggerHandler::OnOpenCalled,
                                weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Reject(const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args->GetList()[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    NS_LOG(ERROR) << "Invalid ShareTarget ID " << share_target_id
                  << " for Reject.";
    return;
  }

  service_->Reject(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnRejectCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyInternalsUiTriggerHandler::Cancel(const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (!service_) {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
    return;
  }

  std::string share_target_id = args->GetList()[0].GetString();
  auto it = id_to_share_target_map_.find(share_target_id);
  if (it == id_to_share_target_map_.end()) {
    NS_LOG(ERROR) << "Invalid ShareTarget ID " << share_target_id
                  << " for Cancel.";
    return;
  }

  service_->Cancel(
      it->second,
      base::BindOnce(&NearbyInternalsUiTriggerHandler::OnCancelCalled,
                     weak_ptr_factory_.GetWeakPtr()));
}
