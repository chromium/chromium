// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_FEATURES_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace unexportable_keys {

// If enabled, DBSC-related code will switch `UnexportableKeyProvider` to a mock
// software-backed implementation.
// This feature flag is expected to never be shipped to end users.
COMPONENT_EXPORT(UNEXPORTABLE_KEYS)
BASE_DECLARE_FEATURE(
    kEnableBoundSessionCredentialsSoftwareKeysForManualTesting);

}  // namespace unexportable_keys
#endif  // COMPONENTS_UNEXPORTABLE_KEYS_FEATURES_H_
