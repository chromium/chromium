// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver.h"

#include <stddef.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

namespace {

// Copied from components/invalidation/impl/fcm_invalidation_service_base.cc.
constexpr char kFcmInvalidationsApplicationName[] =
    "com.google.chrome.fcm.invalidations";
// Copied from components/sync/invalidations/sync_invalidations_service_impl.cc.
constexpr char kSyncInvalidationsApplicationName[] =
    "com.google.chrome.sync.invalidations";

void LogDeliveredToAppHandler(const std::string& app_id, bool has_app_handler) {
  base::UmaHistogramBoolean("GCM.DeliveredToAppHandler", has_app_handler);

  // Record for sync-related app handlers, used to estimate missed sync
  // invalidations.
  if (app_id == kSyncInvalidationsApplicationName) {
    base::UmaHistogramBoolean("GCM.DeliveredToAppHandler.SyncInvalidations",
                              has_app_handler);
  } else if (app_id == kFcmInvalidationsApplicationName) {
    base::UmaHistogramBoolean("GCM.DeliveredToAppHandler.FcmInvalidations",
                              has_app_handler);
  }
}

}  // namespace

InstanceIDHandler::InstanceIDHandler() = default;

InstanceIDHandler::~InstanceIDHandler() = default;

void InstanceIDHandler::DeleteAllTokensForApp(const std::string& app_id,
                                              DeleteTokenCallback callback) {
  DeleteToken(app_id, "*", "*", std::move(callback));
}

GCMDriver::GCMDriver(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  // The |blocking_task_runner| can be nullptr for tests that do not need the
  // encryption capabilities of the GCMDriver class.
  if (blocking_task_runner)
    encryption_provider_.Init(store_path, blocking_task_runner);
}

GCMDriver::~GCMDriver() = default;

void GCMDriver::Register(const std::string& app_id,
                         const std::vector<std::string>& sender_ids,
                         RegisterCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_ids.empty() && sender_ids.size() <= kMaxSenders);
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    std::move(callback).Run(std::string(), result);
    return;
  }

  // If previous register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end()) {
    std::move(callback).Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  register_callbacks_[app_id] = std::move(callback);

  // If previous unregister operation is still in progress, wait until it
  // finishes. We don't want to throw ASYNC_OPERATION_PENDING when the user
  // uninstalls an app (ungistering) and then reinstalls the app again
  // (registering).
  auto unregister_iter = unregister_callbacks_.find(app_id);
  if (unregister_iter != unregister_callbacks_.end()) {
    // Replace the original unregister callback with an intermediate callback
    // that will invoke the original unregister callback and trigger the pending
    // registration after the unregistration finishes.
    // Note that some parameters to RegisterAfterUnregister are specified here
    // when the callback is created (base::Bind supports the partial binding
    // of parameters).
    unregister_iter->second = base::BindOnce(
        &GCMDriver::RegisterAfterUnregister, weak_ptr_factory_.GetWeakPtr(),
        app_id, normalized_sender_ids, std::move(unregister_iter->second));
    return;
  }

  RegisterImpl(app_id, normalized_sender_ids);
}

void GCMDriver::Unregister(const std::string& app_id,
                           UnregisterCallback callback) {
  UnregisterInternal(app_id, nullptr /* sender_id */, std::move(callback));
}

void GCMDriver::UnregisterWithSenderId(const std::string& app_id,
                                       const std::string& sender_id,
                                       UnregisterCallback callback) {
  DCHECK(!sender_id.empty());
  UnregisterInternal(app_id, &sender_id, std::move(callback));
}

void GCMDriver::UnregisterInternal(const std::string& app_id,
                                   const std::string* sender_id,
                                   UnregisterCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    std::move(callback).Run(result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end() ||
      unregister_callbacks_.find(app_id) != unregister_callbacks_.end()) {
    std::move(callback).Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  unregister_callbacks_[app_id] = std::move(callback);

  if (sender_id)
    UnregisterWithSenderIdImpl(app_id, *sender_id);
  else
    UnregisterImpl(app_id);
}

void GCMDriver::Send(const std::string& app_id,
                     const std::string& receiver_id,
                     const OutgoingMessage& message,
                     SendCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!receiver_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    std::move(callback).Run(std::string(), result);
    return;
  }

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    std::move(callback).Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }

  send_callbacks_[key] = std::move(callback);

  SendImpl(app_id, receiver_id, message);
}

void GCMDriver::GetEncryptionInfo(const std::string& app_id,
                                  GetEncryptionInfoCallback callback) {
  encryption_provider_.GetEncryptionInfo(app_id, "" /* authorized_entity */,
                                         std::move(callback));
}

void GCMDriver::UnregisterWithSenderIdImpl(const std::string& app_id,
                                           const std::string& sender_id) {
  NOTREACHED_IN_MIGRATION();
}

void GCMDriver::RegisterFinished(const std::string& app_id,
                                 const std::string& registration_id,
                                 GCMClient::Result result) {
  TRACE_EVENT0("identity", "GCMDriver::RegisterFinished");
  auto callback_iter = register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  RegisterCallback callback = std::move(callback_iter->second);
  register_callbacks_.erase(callback_iter);
  std::move(callback).Run(registration_id, result);
}

void GCMDriver::RemoveEncryptionInfoAfterUnregister(const std::string& app_id,
                                                    GCMClient::Result result) {
  encryption_provider_.RemoveEncryptionInfo(
      app_id, "" /* authorized_entity */,
      base::BindOnce(&GCMDriver::UnregisterFinished,
                     weak_ptr_factory_.GetWeakPtr(), app_id, result));
}

void GCMDriver::UnregisterFinished(const std::string& app_id,
                                   GCMClient::Result result) {
  auto callback_iter = unregister_callbacks_.find(app_id);
  if (callback_iter == unregister_callbacks_.end())
    return;

  UnregisterCallback callback = std::move(callback_iter->second);
  unregister_callbacks_.erase(callback_iter);
  std::move(callback).Run(result);
}

void GCMDriver::SendFinished(const std::string& app_id,
                             const std::string& message_id,
                             GCMClient::Result result) {
  auto callback_iter = send_callbacks_.find(
      std::pair<std::string, std::string>(app_id, message_id));
  if (callback_iter == send_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  SendCallback callback = std::move(callback_iter->second);
  send_callbacks_.erase(callback_iter);
  std::move(callback).Run(message_id, result);
}

void GCMDriver::Shutdown() {
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    DVLOG(1) << "Calling ShutdownHandler for: " << iter->first;
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();
}

void GCMDriver::AddAppHandler(const std::string& app_id,
                              GCMAppHandler* handler) {
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK_EQ(app_handlers_.count(app_id), 0u);
  app_handlers_[app_id] = handler;
  DVLOG(1) << "App handler added for: " << app_id;
}

void GCMDriver::RemoveAppHandler(const std::string& app_id) {
  DCHECK(!app_id.empty());
  app_handlers_.erase(app_id);
  DVLOG(1) << "App handler removed for: " << app_id;
}

GCMAppHandler* GCMDriver::GetAppHandler(const std::string& app_id) {
  // Look for exact match.
  GCMAppHandlerMap::const_iterator iter = app_handlers_.find(app_id);
  if (iter != app_handlers_.end())
    return iter->second;

  // Ask the handlers whether they know how to handle it.
  for (iter = app_handlers_.begin(); iter != app_handlers_.end(); ++iter) {
    if (iter->second->CanHandle(app_id))
      return iter->second;
  }

  return nullptr;
}

GCMEncryptionProvider* GCMDriver::GetEncryptionProviderInternal() {
  return &encryption_provider_;
}

bool GCMDriver::HasRegisterCallback(const std::string& app_id) {
  return register_callbacks_.find(app_id) != register_callbacks_.end();
}

void GCMDriver::ClearCallbacks() {
  register_callbacks_.clear();
  unregister_callbacks_.clear();
  send_callbacks_.clear();
}

void GCMDriver::DispatchMessage(const std::string& app_id,
                                const IncomingMessage& message) {
  encryption_provider_.DecryptMessage(
      app_id, message,
      base::BindOnce(&GCMDriver::DispatchMessageInternal,
                     weak_ptr_factory_.GetWeakPtr(), app_id));
}

void GCMDriver::DispatchMessageInternal(const std::string& app_id,
                                        GCMDecryptionResult result,
                                        IncomingMessage message) {
  UMA_HISTOGRAM_ENUMERATION("GCM.Crypto.DecryptMessageResult", result,
                            GCMDecryptionResult::ENUM_SIZE);

  switch (result) {
    case GCMDecryptionResult::UNENCRYPTED:
    case GCMDecryptionResult::DECRYPTED_DRAFT_03:
    case GCMDecryptionResult::DECRYPTED_DRAFT_08: {
      GCMAppHandler* handler = GetAppHandler(app_id);
      LogDeliveredToAppHandler(app_id, !!handler);

      // TODO(crbug.com/40888673): store incoming messages in memory while
      // AppHandler is not registered.
      if (handler)
        handler->OnMessage(app_id, message);

      // TODO(peter/harkness): Surface unavailable app handlers on
      // chrome://gcm-internals and send a delivery receipt.
      return;
    }
    case GCMDecryptionResult::INVALID_ENCRYPTION_HEADER:
    case GCMDecryptionResult::INVALID_CRYPTO_KEY_HEADER:
    case GCMDecryptionResult::NO_KEYS:
    case GCMDecryptionResult::INVALID_SHARED_SECRET:
    case GCMDecryptionResult::INVALID_PAYLOAD:
    case GCMDecryptionResult::INVALID_BINARY_HEADER_PAYLOAD_LENGTH:
    case GCMDecryptionResult::INVALID_BINARY_HEADER_RECORD_SIZE:
    case GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH:
    case GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT: {
      RecordDecryptionFailure(app_id, result);
      GCMAppHandler* handler = GetAppHandler(app_id);
      if (handler) {
        handler->OnMessageDecryptionFailed(
            app_id, message.message_id,
            ToGCMDecryptionResultDetailsString(result));
      }
      return;
    }
    case GCMDecryptionResult::ENUM_SIZE:
      break;  // deliberate fall-through
  }

  NOTREACHED_IN_MIGRATION();
}

void GCMDriver::RegisterAfterUnregister(
    const std::string& app_id,
    const std::vector<std::string>& normalized_sender_ids,
    UnregisterCallback unregister_callback,
    GCMClient::Result result) {
  // Invoke the original unregister callback.
  std::move(unregister_callback).Run(result);

  // Trigger the pending registration.
  DCHECK(register_callbacks_.find(app_id) != register_callbacks_.end());
  RegisterImpl(app_id, normalized_sender_ids);
}

void GCMDriver::EncryptMessage(const std::string& app_id,
                               const std::string& authorized_entity,
                               const std::string& p256dh,
                               const std::string& auth_secret,
                               const std::string& message,
                               EncryptMessageCallback callback) {
  encryption_provider_.EncryptMessage(
      app_id, authorized_entity, p256dh, auth_secret, message,
      base::BindOnce(&GCMDriver::OnMessageEncrypted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GCMDriver::OnMessageEncrypted(EncryptMessageCallback callback,
                                   GCMEncryptionResult result,
                                   std::string message) {
  std::move(callback).Run(result, std::move(message));
}

void GCMDriver::DecryptMessage(const std::string& app_id,
                               const std::string& authorized_entity,
                               const std::string& message,
                               DecryptMessageCallback callback) {
  IncomingMessage incoming_message;
  incoming_message.sender_id = authorized_entity;
  incoming_message.raw_data = message;
  incoming_message.data[GCMEncryptionProvider::kContentEncodingProperty] =
      GCMEncryptionProvider::kContentCodingAes128Gcm;
  encryption_provider_.DecryptMessage(
      app_id, incoming_message,
      base::BindOnce(&GCMDriver::OnMessageDecrypted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GCMDriver::OnMessageDecrypted(DecryptMessageCallback callback,
                                   GCMDecryptionResult result,
                                   IncomingMessage message) {
  UMA_HISTOGRAM_ENUMERATION("GCM.Crypto.DecryptMessageResult", result,
                            GCMDecryptionResult::ENUM_SIZE);
  std::move(callback).Run(result, std::move(message.raw_data));
}

}  // namespace gcm
