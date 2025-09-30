// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_FEATURES_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace data_sharing::migration {

// Enables the client-side migration framework for sharing Tab Groups.
BASE_DECLARE_FEATURE(kEnableTabGroupSharingMigration);

}  // namespace data_sharing::migration

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_FEATURES_H_
