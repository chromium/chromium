// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

class JsEventDetails;
class JsEventHandler;

// Routes SyncEncryptionHandler events to a JsEventHandler.
class JsSyncEncryptionHandlerObserver : public SyncEncryptionHandler::Observer {
 public:
  JsSyncEncryptionHandlerObserver();
  ~JsSyncEncryptionHandlerObserver() override;

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // SyncEncryptionHandlerObserver::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

 private:
  void HandleJsEvent(const base::Location& from_here,
                     const std::string& name,
                     const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncEncryptionHandlerObserver);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_
