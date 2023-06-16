// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_PENDING_LOCAL_NIGORI_COMMIT_H_
#define COMPONENTS_SYNC_NIGORI_PENDING_LOCAL_NIGORI_COMMIT_H_

#include <memory>
#include <string>

#include "components/sync/engine/sync_encryption_handler.h"

namespace syncer {

class KeyDerivationParams;
struct NigoriState;

// Interface representing an intended local change to the Nigori state that
// is pending a commit to the sync server.
class PendingLocalNigoriCommit {
 public:
  static std::unique_ptr<PendingLocalNigoriCommit> ForSetCustomPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params);

  static std::unique_ptr<PendingLocalNigoriCommit> ForKeystoreInitialization();

  static std::unique_ptr<PendingLocalNigoriCommit> ForKeystoreReencryption();

  static std::unique_ptr<PendingLocalNigoriCommit>
  ForCrossUserSharingPublicPrivateKeyInitializer();

  PendingLocalNigoriCommit() = default;

  PendingLocalNigoriCommit(const PendingLocalNigoriCommit&) = delete;
  PendingLocalNigoriCommit& operator=(const PendingLocalNigoriCommit&) = delete;

  virtual ~PendingLocalNigoriCommit() = default;

  // Attempts to modify |*state| to reflect the intended commit. Returns true if
  // the change was successfully applied (which may include the no-op case) or
  // false if it no longer applies (leading to OnFailure()).
  //
  // |state| must not be null.
  virtual bool TryApply(NigoriState* state) const = 0;

  // Invoked when the commit has been successfully acked by the server.
  // |observer| must not be null.
  virtual void OnSuccess(const NigoriState& state,
                         SyncEncryptionHandler::Observer* observer) = 0;

  // Invoked when the change no longer applies or was aborted for a different
  // reason (e.g. sync disabled). |observer| must not be null.
  virtual void OnFailure(SyncEncryptionHandler::Observer* observer) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_PENDING_LOCAL_NIGORI_COMMIT_H_
