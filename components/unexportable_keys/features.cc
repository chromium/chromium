// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/features.h"

namespace unexportable_keys {

BASE_FEATURE(kEnableBoundSessionCredentialsSoftwareKeysForManualTesting,
             "EnableBoundSessionCredentialsSoftwareKeysForManualTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace unexportable_keys
