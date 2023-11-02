// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_KEYSTORE_KEYS_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_KEYSTORE_KEYS_HANDLER_H_

#include <vector>

namespace syncer {

// Sync internal interface for dealing with Nigori keystore keys.
class KeystoreKeysHandler {
 public:
  KeystoreKeysHandler() = default;

  KeystoreKeysHandler(const KeystoreKeysHandler&) = delete;
  KeystoreKeysHandler& operator=(const KeystoreKeysHandler&) = delete;

  virtual ~KeystoreKeysHandler() = default;

  // Whether a keystore key needs to be requested from the sync server.
  virtual bool NeedKeystoreKey() const = 0;

  // Sets the keystore keys the server returned for this account.
  // Returns true on success, false otherwise.
  virtual bool SetKeystoreKeys(
      const std::vector<std::vector<uint8_t>>& keys) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_KEYSTORE_KEYS_HANDLER_H_
