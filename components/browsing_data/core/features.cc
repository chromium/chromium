// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/features.h"

#include "build/build_config.h"

namespace browsing_data::features {
BASE_FEATURE(kEnableBrowsingDataLifetimeManager,
             "BrowsingDataLifetimeManager",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecateCookiesTreeModel,
             "DeprecateCookiesTreeModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMigrateStorageToBDM,
             "MigrateStorageToBDM",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDataRetentionPoliciesDisableSyncTypesNeeded,
             "DataRetentionPoliciesDisableSyncTypesNeeded",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace browsing_data::features
