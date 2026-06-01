// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_settings/account_settings_features.h"

namespace account_settings::features {

// Kill switch to gate context-related account settings.
BASE_FEATURE(kAccountSettingContextKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace account_settings::features
