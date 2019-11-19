// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/js_sync_encryption_handler_observer.h"

#include <cstddef>

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_event_handler.h"
#include "components/sync/nigori/cryptographer.h"

namespace syncer {

JsSyncEncryptionHandlerObserver::JsSyncEncryptionHandlerObserver() {}

JsSyncEncryptionHandlerObserver::~JsSyncEncryptionHandlerObserver() {}

void JsSyncEncryptionHandlerObserver::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  event_handler_ = event_handler;
}

void JsSyncEncryptionHandlerObserver::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetString("reason", PassphraseRequiredReasonToString(reason));
  HandleJsEvent(FROM_HERE, "onPassphraseRequired", JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnPassphraseAccepted() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  HandleJsEvent(FROM_HERE, "onPassphraseAccepted", JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnTrustedVaultKeyRequired() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  HandleJsEvent(FROM_HERE, "OnTrustedVaultKeyRequired",
                JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnTrustedVaultKeyAccepted() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  HandleJsEvent(FROM_HERE, "OnTrustedVaultKeyAccepted",
                JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnBootstrapTokenUpdated(
    const std::string& boostrap_token,
    BootstrapTokenType type) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetString("bootstrapToken", "<redacted>");
  details.SetString("type", BootstrapTokenTypeToString(type));
  HandleJsEvent(FROM_HERE, "onBootstrapTokenUpdated", JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.Set("encryptedTypes", ModelTypeSetToValue(encrypted_types));
  details.SetBoolean("encryptEverything", encrypt_everything);
  HandleJsEvent(FROM_HERE, "onEncryptedTypesChanged", JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnEncryptionComplete() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  HandleJsEvent(FROM_HERE, "onEncryptionComplete", JsEventDetails());
}

void JsSyncEncryptionHandlerObserver::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetBoolean("canEncrypt", cryptographer->CanEncrypt());
  details.SetBoolean("hasPendingKeys", has_pending_keys);
  HandleJsEvent(FROM_HERE, "onCryptographerStateChanged",
                JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  base::DictionaryValue details;
  details.SetString("passphraseType", PassphraseTypeToString(type));
  details.SetInteger("explicitPassphraseTime",
                     TimeToProtoTime(explicit_passphrase_time));
  HandleJsEvent(FROM_HERE, "onPassphraseTypeChanged", JsEventDetails(&details));
}

void JsSyncEncryptionHandlerObserver::HandleJsEvent(
    const base::Location& from_here,
    const std::string& name,
    const JsEventDetails& details) {
  if (!event_handler_.IsInitialized()) {
    NOTREACHED();
    return;
  }
  event_handler_.Call(from_here, &JsEventHandler::HandleJsEvent, name, details);
}

}  // namespace syncer
