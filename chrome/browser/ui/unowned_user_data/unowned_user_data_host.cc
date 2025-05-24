// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unowned_user_data/unowned_user_data_host.h"

#include <ostream>

#include "base/check.h"

UnownedUserDataHost::UnownedUserDataHost() = default;

UnownedUserDataHost::~UnownedUserDataHost() {
  // All UnownedUserData should be removed before the host is destroyed;
  // otherwise, there could be a UAF when they try to remove themselves as they
  // are destroyed.
  // If any remain, print out the first entry's key. There should never be any,
  // so this should be sufficient to help folks debug.
  CHECK(map_.empty()) << "All UnownedUserData must be removed before the "
                      << "corresponding UnownedUserDataHost is destroyed. "
                      << "First remaining key: " << map_.begin()->first;
}

void UnownedUserDataHost::Set(
    base::PassKey<internal::ScopedUnownedUserDataBase> pass_key,
    const char* key,
    void* data) {
  CHECK(data) << "Assigning bad data for key: " << key;
  bool inserted = map_.emplace(key, data).second;
  CHECK(inserted) << "Attempted to reinsert data for key: " << key;
}

void UnownedUserDataHost::Erase(
    base::PassKey<internal::ScopedUnownedUserDataBase> pass_key,
    const char* key) {
  CHECK(map_.contains(key)) << "Erasing invalid data for key: " << key;
  map_.erase(key);
}

void* UnownedUserDataHost::Get(
    base::PassKey<internal::ScopedUnownedUserDataBase>,
    const char* key) {
  auto iter = map_.find(key);
  return iter == map_.end() ? nullptr : iter->second;
}
