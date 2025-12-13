// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/features.h"

#include "base/feature_list.h"

namespace credential_management::features {

// If the user configured Chrome to use 3P autofill and this feature is enabled,
// Chrome can forward the Credential Management API requests to 3P password
// managers.
BASE_FEATURE(kCredentialManagementThirdPartyWebApiRequestForwarding,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace credential_management::features
