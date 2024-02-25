// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumbs_status.h"

#include <atomic>
#include <optional>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/channel.h"

namespace breadcrumbs {

namespace {

// Represents whether breadcrumbs is enabled/disabled from local prefs (the
// default and most common case) or forced to enabled/disabled.
enum class BreadcrumbsEnabledMode {
  kFromLocalPrefs,
  kForceEnabled,
  kForceDisabled
};

// If set to `kFromLocalPrefs`, breadcrumbs is enabled or disabled based on
// Local State prefs like usual. Otherwise, breadcrumbs has been forced either
// on (e.g., for testing) or off (e.g., for Chrome on Android's minimal mode).
std::atomic<BreadcrumbsEnabledMode> breadcrumbs_enabled_mode =
    BreadcrumbsEnabledMode::kFromLocalPrefs;

// The percentage at which breadcrumbs is enabled per channel.
// Enable on most pre-Stable clients to ensure crashes with only a few reports
// have enough breadcrumbs to be useful.
constexpr int kCanaryPercent = 99;
constexpr int kDevPercent = 80;
constexpr int kBetaPercent = 80;
// Enable on a small portion of Stable. This rate is expected to provide enough
// breadcrumbs data while not logging on more clients than necessary.
constexpr int kStablePercent = 5;

// How long logging should be enabled on a client before re-randomizing.
constexpr auto kEnabledDuration = base::Days(30);

// Returns a random boolean representing whether breadcrumbs should be enabled
// for `channel`. For example, if logging is enabled at 5% on `channel`, this
// has a 5% chance of returning true.
bool GetRandomIsEnabled(version_info::Channel channel) {
  int enabled_percent = 0;
  switch (channel) {
    case version_info::Channel::CANARY:
      enabled_percent = kCanaryPercent;
      break;
    case version_info::Channel::DEV:
      enabled_percent = kDevPercent;
      break;
    case version_info::Channel::BETA:
      enabled_percent = kBetaPercent;
      break;
    case version_info::Channel::STABLE:
      enabled_percent = kStablePercent;
      break;
    case version_info::Channel::UNKNOWN:
      break;
  }
  return base::RandInt(1, 100) <= enabled_percent;
}

// Returns true if `prefs` contains both breadcrumbs prefs, and the timestamp is
// valid and newer than `kEnabledDuration`.
bool HasRecentBreadcrumbsPrefs(PrefService* prefs) {
  if (!prefs->HasPrefPath(kEnabledPref) ||
      !prefs->HasPrefPath(kEnabledTimePref)) {
    // Breadcrumbs prefs have never been set.
    return false;
  }
  const auto enabled_time = prefs->GetTime(kEnabledTimePref);
  const auto now = base::Time::Now();
  if (enabled_time > now) {
    // Timestamp is in the future, so consider it invalid.
    return false;
  }
  const auto oldest_valid_time = now - kEnabledDuration;
  return enabled_time > oldest_valid_time;
}

bool IsEnabled(PrefService* prefs,
               std::optional<version_info::Channel> set_for_channel) {
  switch (breadcrumbs_enabled_mode) {
    case BreadcrumbsEnabledMode::kForceEnabled:
      return true;
    case BreadcrumbsEnabledMode::kForceDisabled:
      return false;
    case BreadcrumbsEnabledMode::kFromLocalPrefs:
      // `prefs` can be null or unregistered in tests, and can be null in Chrome
      // for Android's minimal mode. In these cases, breadcrumbs should be
      // disabled for the entire session. Also, breadcrumbs should be disabled
      // while benchmarking, to reflect the typical client experience.
      if (!prefs || !prefs->FindPreference(kEnabledPref) ||
          !prefs->FindPreference(kEnabledTimePref) ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              variations::switches::kEnableBenchmarking)) {
        breadcrumbs_enabled_mode = BreadcrumbsEnabledMode::kForceDisabled;
        return false;
      }

      if (set_for_channel.has_value()) {
        // Keep breadcrumbs consistently enabled/disabled on a given client for
        // `kEnabledDuration`. If breadcrumbs has been enabled/disabled in prefs
        // more recently than `kEnabledDuration`, use the existing setting.
        if (HasRecentBreadcrumbsPrefs(prefs)) {
          return prefs->GetBoolean(kEnabledPref);
        }

        // Re-randomize if breadcrumbs was enabled/disabled too long ago. This
        // either enables or disables breadcrumbs for `kEnabledDuration`.
        const bool is_enabled = GetRandomIsEnabled(set_for_channel.value());
        prefs->SetBoolean(kEnabledPref, is_enabled);
        prefs->SetTime(kEnabledTimePref, base::Time::Now());
      }

      return prefs->GetBoolean(kEnabledPref);
  }
}

}  // namespace

constexpr char kEnabledPref[] = "breadcrumbs.enabled";
constexpr char kEnabledTimePref[] = "breadcrumbs.enabled_time";

bool IsEnabled(PrefService* prefs) {
  return IsEnabled(prefs, std::nullopt);
}

bool MaybeEnableBasedOnChannel(PrefService* prefs,
                               version_info::Channel channel) {
  return IsEnabled(prefs, channel);
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kEnabledPref, false);
  registry->RegisterTimePref(kEnabledTimePref, base::Time());
}

ScopedEnableBreadcrumbsForTesting::ScopedEnableBreadcrumbsForTesting() {
  breadcrumbs_enabled_mode = BreadcrumbsEnabledMode::kForceEnabled;
}

ScopedEnableBreadcrumbsForTesting::~ScopedEnableBreadcrumbsForTesting() {
  breadcrumbs_enabled_mode = BreadcrumbsEnabledMode::kFromLocalPrefs;
}

}  // namespace breadcrumbs
