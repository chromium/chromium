// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/settings.h"

#include "components/enterprise/isolated_mode/isolated_mode_settings_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"

namespace enterprise_isolated_mode {

bool IsolatedModeReplacesIncognito(const PrefService& pref_service,
                                   version_info::Channel channel) {
  return IsolatedModeSettingsService(&const_cast<PrefService&>(pref_service),
                                     channel)
      .ReplacesIncognito();
}

}  // namespace enterprise_isolated_mode
