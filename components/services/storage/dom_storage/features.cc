// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/features.h"

namespace storage {

BASE_FEATURE(kDomStorageSqlite, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDomStorageSqliteInMemory, base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldUseSqliteBackend(bool is_in_memory) {
  if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
    return true;
  }
  if (is_in_memory) {
    return base::FeatureList::IsEnabled(kDomStorageSqliteInMemory);
  }
  return false;
}

}  // namespace storage
