// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/featured/featured_client.h"
#include "components/variations/cros/featured.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace variations {

// Consecutive seed fetch failures are, unfortunately, a bit more common. As of
// January 2018, users at the 99.5th percentile tend to see fewer than 4
// consecutive fetch failures on mobile platforms; and users at the 99th
// percentile tend to see fewer than 5 or 6 consecutive failures on desktop
// platforms. It makes sense that the characteristics differ on mobile
// vs. desktop platforms, given that the two use different scheduling algorithms
// for the fetches. Graphs:
// [1] Android, all channels (consistently connected):
//     https://uma.googleplex.com/timeline_v2?sid=99d1d4c2490c60bcbde7afeb77c12a28
// [2] High-connectivity platforms, Stable and Beta channel (consistently
//     connected):
//     https://uma.googleplex.com/timeline_v2?sid=2db5b7278dad41cbf349f5f2cb30efd9
// [3] Other platforms, Stable and Beta channel (slightly less connected):
//     https://uma.googleplex.com/timeline_v2?sid=d4ba2f3751d211898f8e69214147c2ec
// [4] All platforms, Dev (even less connected):
//     https://uma.googleplex.com/timeline_v2?sid=5740fb22b17faa823822adfd8e00ec1a
// [5] All platforms, Canary (actually fairly well-connected!):
//     https://uma.googleplex.com/timeline_v2?sid=3e14d3e4887792bb614db9f3f2c1d48c
// Note the all of the graphs show a spike on a particular day, presumably due
// to server-side instability. Moreover, the Dev channel on desktop is an
// outlier – users on the Dev channel can experience just shy of 9 consecutive
// failures on some platforms.
// Decision: There is not an obvious threshold that both achieves a low
// false-positive rate and provides good coverage for true positives. For now,
// set a threshold that should minimize false-positives.
// TODO(isherman): Check in with the networking team about their thoughts on how
// to find a better balance here.
constexpr int kFetchFailureStreakSafeSeedThreshold = 25;
constexpr int kFetchFailureStreakNullSeedThreshold = 50;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Number of attempts to send the safe seed from Chrome to CrOS platforms before
// giving up.
constexpr int kSendPlatformSafeSeedMaxAttempts = 2;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

SafeSeedManager::SafeSeedManager(PrefService* local_state)
    : local_state_(local_state) {
  int num_failed_fetches =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  base::UmaHistogramSparse("Variations.SafeMode.Streak.FetchFailures",
                           std::clamp(num_failed_fetches, 0, 100));
}

SafeSeedManager::~SafeSeedManager() = default;

// static
void SafeSeedManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // Verify that the crash streak pref has already been registered.
  DCHECK(
      registry->defaults()->GetValue(prefs::kVariationsCrashStreak, nullptr));

  // Registers one of two prefs used for tracking variations-seed-related
  // failures. The other pref, kVariationsCrashStreak, is registered in
  // CleanExitBeacon::RegisterPrefs(). See components/metrics/
  // clean_exit_beacon.cc for more details.
  registry->RegisterIntegerPref(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

SeedType SafeSeedManager::GetSeedType() const {
  // Ignore any number of failures if the --disable-variations-safe-mode flag is
  // set.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVariationsSafeMode)) {
    return SeedType::kRegularSeed;
  }
  int num_crashes = local_state_->GetInteger(prefs::kVariationsCrashStreak);
  int num_failed_fetches =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  if (num_crashes >= kCrashStreakNullSeedThreshold ||
      num_failed_fetches >= kFetchFailureStreakNullSeedThreshold) {
#if BUILDFLAG(IS_CHROMEOS)
    // Logging is useful in listnr reports for ChromeOS (http://b/277650823).
    LOG(ERROR) << "Using finch safe mode null seed: num_crashes=" << num_crashes
               << ", num_failed_fetches=" << num_failed_fetches;
#endif  // BUILDFLAG(IS_CHROMEOS)
    return SeedType::kNullSeed;
  }
  if (num_crashes >= kCrashStreakSafeSeedThreshold ||
      num_failed_fetches >= kFetchFailureStreakSafeSeedThreshold) {
#if BUILDFLAG(IS_CHROMEOS)
    LOG(ERROR) << "Using finch safe mode safe seed: num_crashes=" << num_crashes
               << ", num_failed_fetches=" << num_failed_fetches;
#endif  // BUILDFLAG(IS_CHROMEOS)
    return SeedType::kSafeSeed;
  }
  return SeedType::kRegularSeed;
}

void SafeSeedManager::SetActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    int seed_milestone,
    std::unique_ptr<ClientFilterableState> client_filterable_state,
    base::Time seed_fetch_time) {
  DCHECK(!has_set_active_seed_state_);
  has_set_active_seed_state_ = true;

  active_seed_state_ = std::make_unique<ActiveSeedState>(
      seed_data, base64_seed_signature, seed_milestone,
      std::move(client_filterable_state), seed_fetch_time);
}

void SafeSeedManager::RecordFetchStarted() {
  // Pessimistically assume the fetch will fail. The failure streak will be
  // reset upon success.
  int num_failures_to_fetch =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak,
                           num_failures_to_fetch + 1);
}

void SafeSeedManager::RecordSuccessfulFetch(VariationsSeedStore* seed_store) {
  // The first time a fetch succeeds for a given run of Chrome, save the active
  // seed+filter configuration as safe. Note that it's sufficient to do this
  // only on the first successful fetch because the active configuration does
  // not change while Chrome is running. Also, note that it's fine to do this
  // even if running in safe mode, as the saved seed in that case will just be
  // the existing safe seed.
  if (active_seed_state_) {
    seed_store->StoreSafeSeed(active_seed_state_->seed_data,
                              active_seed_state_->base64_seed_signature,
                              active_seed_state_->seed_milestone,
                              *active_seed_state_->client_filterable_state,
                              active_seed_state_->seed_fetch_time);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // `SendSafeSeedToPlatform` will send the safe seed at most twice.
    // This is a best effort attempt and it is possible that the safe seed for
    // platform and Chrome are different if sending the safe seed fails twice.
    send_seed_to_platform_attempts_ = 0;
    SendSafeSeedToPlatform(GetSafeSeedStateForPlatform());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // The active seed state is only needed for the first time this code path is
    // reached, so free up its memory once the data is no longer needed.
    active_seed_state_.reset();
  }

  // Note: It's important to clear the crash streak as well as the fetch
  // failures streak. Crashes that occur after a successful seed fetch do not
  // prevent updating to a new seed, and therefore do not necessitate falling
  // back to a safe seed.
  local_state_->SetInteger(prefs::kVariationsCrashStreak, 0);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

SafeSeedManager::ActiveSeedState::ActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    int seed_milestone,
    std::unique_ptr<ClientFilterableState> client_filterable_state,
    base::Time seed_fetch_time)
    : seed_data(seed_data),
      base64_seed_signature(base64_seed_signature),
      seed_milestone(seed_milestone),
      client_filterable_state(std::move(client_filterable_state)),
      seed_fetch_time(seed_fetch_time) {}

SafeSeedManager::ActiveSeedState::~ActiveSeedState() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
featured::SeedDetails SafeSeedManager::GetSafeSeedStateForPlatform() {
  featured::SeedDetails safe_seed;
  safe_seed.set_compressed_data(active_seed_state_->seed_data);
  safe_seed.set_locale(active_seed_state_->client_filterable_state->locale);
  safe_seed.set_milestone(active_seed_state_->seed_milestone);
  safe_seed.set_permanent_consistency_country(
      active_seed_state_->client_filterable_state
          ->permanent_consistency_country);
  safe_seed.set_session_consistency_country(
      active_seed_state_->client_filterable_state->session_consistency_country);
  safe_seed.set_signature(active_seed_state_->base64_seed_signature);
  safe_seed.set_date(active_seed_state_->client_filterable_state->reference_date
                         .ToDeltaSinceWindowsEpoch()
                         .InMilliseconds());
  safe_seed.set_fetch_time(
      active_seed_state_->seed_fetch_time.ToDeltaSinceWindowsEpoch()
          .InMilliseconds());

  return safe_seed;
}

void SafeSeedManager::MaybeRetrySendSafeSeed(
    const featured::SeedDetails& safe_seed,
    bool success) {
  // Do not retry after two failed attempts.
  if (!success &&
      send_seed_to_platform_attempts_ < kSendPlatformSafeSeedMaxAttempts) {
    SendSafeSeedToPlatform(safe_seed);
  }
}

void SafeSeedManager::SendSafeSeedToPlatform(
    const featured::SeedDetails& safe_seed) {
  send_seed_to_platform_attempts_++;
  ash::featured::FeaturedClient* client = ash::featured::FeaturedClient::Get();
  if (client) {
    client->HandleSeedFetched(
        safe_seed, base::BindOnce(&SafeSeedManager::MaybeRetrySendSafeSeed,
                                  weak_ptr_factory_.GetWeakPtr(), safe_seed));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace variations
