// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/isolated_mode_settings_service.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"

namespace enterprise_isolated_mode {

namespace {

bool IsCommandLineSwitchSupported(version_info::Channel channel) {
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

bool ComputeReplacesIncognito(const PrefService* prefs,
                              version_info::Channel channel) {
  if (IsCommandLineSwitchSupported(channel) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceEnterpriseIsolatedModeReplacesIncognito)) {
    return true;
  }

  if (!prefs || !base::FeatureList::IsEnabled(kEnableEnterpriseIsolatedMode)) {
    return false;
  }

  auto setting = static_cast<IsolatedModeSetting>(
      prefs->GetInteger(kEnterpriseIsolatedModeSettings));
  return setting == IsolatedModeSetting::kEnabled;
}

}  // namespace

IsolatedModeSettingsService::IsolatedModeSettingsService(
    PrefService* prefs,
    version_info::Channel channel)
    // Evaluated exactly once at service creation, natively tying the cached
    // evaluation to the Profile lifecycle and locking in the channel state.
    : replaces_incognito_(ComputeReplacesIncognito(prefs, channel)) {}

}  // namespace enterprise_isolated_mode
