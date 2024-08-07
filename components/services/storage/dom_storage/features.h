// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_

#include "base/features.h"

namespace storage {

BASE_DECLARE_FEATURE(kCoalesceStorageAreaCommits);

// Clears local storage for opaque origins used in prior browsing sessions as
// they will no longer be reachable. See crbug.com/40281870 for more info.
// If kDeleteStaleLocalStorageOnStartup is off this has no impact.
BASE_DECLARE_FEATURE(kDeleteOrphanLocalStorageOnStartup);

// Clears local storage last accessed/modified more than 400 days ago on
// startup. See crbug.com/40281870 for more info.
BASE_DECLARE_FEATURE(kDeleteStaleLocalStorageOnStartup);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
