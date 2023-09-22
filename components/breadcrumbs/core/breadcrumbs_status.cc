// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumbs_status.h"

#include <atomic>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace breadcrumbs {

namespace {

// If true, breadcrumbs is forced to enabled for testing purposes. If false,
// breadcrumbs is in its default state.
std::atomic<bool> is_enabled_for_testing = false;

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
               absl::optional<version_info::Channel> set_for_channel) {
  if (is_enabled_for_testing) {
    return true;
  }
  // `prefs` can be null or not registered in tests.
  if (!prefs || !prefs->FindPreference(kEnabledPref) ||
      !prefs->FindPreference(kEnabledTimePref)) {
    CHECK_IS_TEST();
    return false;
  }
  // Disable breadcrumbs during benchmarking, as it's disabled for most clients.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking)) {
    return false;
  }

  if (set_for_channel.has_value()) {
    // Keep breadcrumbs consistently enabled or disabled on a given client for
    // `kEnabledDuration`. If breadcrumbs has been enabled or disabled in prefs
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

}  // namespace

constexpr char kEnabledPref[] = "breadcrumbs.enabled";
constexpr char kEnabledTimePref[] = "breadcrumbs.enabled_time";

bool IsEnabled(PrefService* prefs) {
  return IsEnabled(prefs, absl::nullopt);
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
  is_enabled_for_testing = true;
}

ScopedEnableBreadcrumbsForTesting::~ScopedEnableBreadcrumbsForTesting() {
  is_enabled_for_testing = false;
}

}  // namespace breadcrumbs
