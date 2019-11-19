// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_HANDLER_H_
#define COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace syncer {

// Sync internal interface for dealing with Nigori keystore keys.
class KeystoreKeysHandler {
 public:
  KeystoreKeysHandler() = default;
  virtual ~KeystoreKeysHandler() = default;

  // Whether a keystore key needs to be requested from the sync server.
  virtual bool NeedKeystoreKey() const = 0;

  // Sets the keystore keys the server returned for this account.
  // Returns true on success, false otherwise.
  virtual bool SetKeystoreKeys(const std::vector<std::string>& keys) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeystoreKeysHandler);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_HANDLER_H_
