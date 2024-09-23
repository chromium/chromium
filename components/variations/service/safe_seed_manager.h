// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
#define COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/variations/service/safe_seed_manager_base.h"

class PrefRegistrySimple;
class PrefService;

namespace variations {

class VariationsSeedStore;

// As of January 2018, users at the 99.5th percentile, across all platforms,
// tend to experience fewer than 3 consecutive crashes: [1], [2], [3], [4].
// Note, however, that this is less true for the less-stable channels on some
// platforms.
// [1] All platforms, stable channel (consistently stable):
//     https://uma.googleplex.com/timeline_v2?sid=90ac80f4573249fb341a8e49501bfcfd
// [2] Most platforms, all channels (consistently stable other than occasional
//     spikes on Canary):
//     https://uma.googleplex.com/timeline_v2?sid=7af5ba1969db76689a401f982a1db539
// [3] A less stable platform, all channels:
//     https://uma.googleplex.com/timeline_v2?sid=07dbc8e4fa9f08e332fb609309a21882
// [4] Another less stable platform, all channels:
//     https://uma.googleplex.com/timeline_v2?sid=a7b529ef5d52863fae2d216e963c4cbc
// Overall, the only {platform, channel} combinations that spike above 3
// consecutive crashes are ones with very few users, plus Canary. It's probably
// not realistic to avoid false positives for these less-stable configurations.
constexpr int kCrashStreakSafeSeedThreshold = 3;
constexpr int kCrashStreakNullSeedThreshold = 4;

// The primary class that encapsulates state for managing the safe seed.
class SafeSeedManager : public SafeSeedManagerBase {
 public:
  // Creates a SafeSeedManager instance and updates a safe mode pref,
  // kVariationsFailedToFetchSeedStreak, for bookkeeping.
  explicit SafeSeedManager(PrefService* local_state);

  SafeSeedManager(const SafeSeedManager&) = delete;
  SafeSeedManager& operator=(const SafeSeedManager&) = delete;

  ~SafeSeedManager() override;

  // Registers safe mode prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the type of seed the client should use.  Uses Regular seed by
  // default, but will use Safe seed, and Null seed after continual crashes or
  // network fetch failures.
  SeedType GetSeedType() const override;

  // Records that a fetch has started: pessimistically increments the
  // corresponding failure streak for safe mode.
  void RecordFetchStarted() override;

  // Records a successful fetch: resets the failure streaks for safe mode.
  // Writes the currently active seed to the |seed_store| as a safe seed, if
  // appropriate.
  void RecordSuccessfulFetch(VariationsSeedStore* seed_store) override;

 private:
  // The pref service used to persist the variations seed. Weak reference; must
  // outlive |this| instance.
  raw_ptr<PrefService> local_state_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
