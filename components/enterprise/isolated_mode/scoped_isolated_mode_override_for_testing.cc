// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/scoped_isolated_mode_override_for_testing.h"

#include "base/check_deref.h"
#include "components/enterprise/isolated_mode/isolated_mode_settings_service.h"

namespace enterprise_isolated_mode {

ScopedIsolatedModeOverrideForTesting::ScopedIsolatedModeOverrideForTesting(
    bool replaces_incognito)
    : default_override_(
          IsolatedModeSettingsService::SetDefaultReplacesIncognitoForTesting(
              replaces_incognito)) {}

ScopedIsolatedModeOverrideForTesting::ScopedIsolatedModeOverrideForTesting(
    IsolatedModeSettingsService* service,
    bool replaces_incognito)
    : service_override_(
          base::AutoReset<bool>(&CHECK_DEREF(service).replaces_incognito_,
                                replaces_incognito)) {}

ScopedIsolatedModeOverrideForTesting::~ScopedIsolatedModeOverrideForTesting() =
    default;

}  // namespace enterprise_isolated_mode
