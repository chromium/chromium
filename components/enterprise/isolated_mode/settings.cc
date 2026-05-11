// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/settings.h"

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

}  // namespace

bool IsolatedModeReplacesIncognito(const PrefService& pref_service,
                                   version_info::Channel channel) {
  if (IsCommandLineSwitchSupported(channel) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceEnterpriseIsolatedModeReplacesIncognito)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(kEnableEnterpriseIsolatedMode)) {
    return false;
  }

  // Treat non-zero as enabled (since it's registered as an integer).
  return pref_service.GetInteger(kEnterpriseIsolatedModeSettings) != 0;
}

}  // namespace enterprise_isolated_mode
