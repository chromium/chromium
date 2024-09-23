// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_VAPID_KEY_MANAGER_H_
#define COMPONENTS_SHARING_MESSAGE_VAPID_KEY_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace syncer {
class SyncService;
}  // namespace syncer

class SharingSyncPreference;
enum class SharingVapidKeyCreationResult;

// Responsible for creating, storing and managing VAPID key. VAPID key is
// shared across all devices for a single user and is used for signing VAPID
// headers for Web Push.
// Web Push Protocol :
// https://developers.google.com/web/fundamentals/push-notifications/web-push-protocol
class VapidKeyManager {
 public:
  explicit VapidKeyManager(SharingSyncPreference* sharing_sync_preference,
                           syncer::SyncService* sync_service);

  VapidKeyManager(const VapidKeyManager&) = delete;
  VapidKeyManager& operator=(const VapidKeyManager&) = delete;

  virtual ~VapidKeyManager();

  // Returns the cached key. If absent, first attempts to refresh the cached
  // key. May return nullptr if cached key is absent after refresh.
  virtual crypto::ECPrivateKey* GetOrCreateKey();

  // Attempts to refresh cached key and returns if cached key has changed:
  // 1. Attempts to cache the key stored in sync prefernece.
  // 2. If failed and cahced key is absent, attempts to generate a new key. If
  // successful, cache it and store in sync preference.
  bool RefreshCachedKey();

 private:
  // Attempts to update cached key if |new_key| is different from cached
  // key, and store updated key to sync preferences. Returns true if cached
  // key is updated.
  bool UpdateCachedKey(std::unique_ptr<crypto::ECPrivateKey> new_key);

  // Attempts to update cached key with key stored in sync preferences. Returns
  // true if cached key is updated.
  bool InitWithPreference();

  // Used for storing and fetching VAPID key from preferences.
  raw_ptr<SharingSyncPreference> sharing_sync_preference_;
  raw_ptr<syncer::SyncService> sync_service_;
  std::unique_ptr<crypto::ECPrivateKey> vapid_key_;
  std::vector<uint8_t> vapid_key_info_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_VAPID_KEY_MANAGER_H_
