// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/storage_key.h"

#include "url/gurl.h"

namespace storage {

StorageKey StorageKey::Deserialize(const std::string& in) {
  return StorageKey(url::Origin::Create(GURL(in)));
}

std::string StorageKey::Serialize() const {
  DCHECK(!opaque());
  return origin_.GetURL().spec();
}

bool operator==(const StorageKey& lhs, const StorageKey& rhs) {
  return lhs.origin_ == rhs.origin_;
}

bool operator!=(const StorageKey& lhs, const StorageKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const StorageKey& lhs, const StorageKey& rhs) {
  return lhs.origin_ < rhs.origin_;
}

}  // namespace storage
