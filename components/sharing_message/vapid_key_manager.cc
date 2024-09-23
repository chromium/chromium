// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/vapid_key_manager.h"

#include "base/feature_list.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sync/service/sync_service.h"
#include "crypto/ec_private_key.h"

VapidKeyManager::VapidKeyManager(SharingSyncPreference* sharing_sync_preference,
                                 syncer::SyncService* sync_service)
    : sharing_sync_preference_(sharing_sync_preference),
      sync_service_(sync_service) {}

VapidKeyManager::~VapidKeyManager() = default;

crypto::ECPrivateKey* VapidKeyManager::GetOrCreateKey() {
  if (!vapid_key_) {
    RefreshCachedKey();
  }

  return vapid_key_.get();
}

bool VapidKeyManager::RefreshCachedKey() {
  if (InitWithPreference()) {
    return true;
  }

  if (vapid_key_) {
    return false;
  }

  // Don't generate keys if preferences is not syncing to avoid isolated keys.
  if (!sync_service_->GetActiveDataTypes().Has(syncer::PREFERENCES)) {
    return false;
  }

  auto generated_key = crypto::ECPrivateKey::Create();
  if (!generated_key) {
    return false;
  }

  return UpdateCachedKey(std::move(generated_key));
}

bool VapidKeyManager::UpdateCachedKey(
    std::unique_ptr<crypto::ECPrivateKey> new_key) {
  std::vector<uint8_t> new_key_info;
  if (!new_key->ExportPrivateKey(&new_key_info)) {
    return false;
  }

  if (vapid_key_info_ == new_key_info) {
    return false;
  }

  vapid_key_ = std::move(new_key);
  vapid_key_info_ = std::move(new_key_info);
  sharing_sync_preference_->SetVapidKey(vapid_key_info_);
  return true;
}

bool VapidKeyManager::InitWithPreference() {
  std::optional<std::vector<uint8_t>> preference_key_info =
      sharing_sync_preference_->GetVapidKey();
  if (!preference_key_info || vapid_key_info_ == *preference_key_info) {
    return false;
  }

  vapid_key_ =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(*preference_key_info);
  vapid_key_info_ = std::move(*preference_key_info);
  return true;
}
