// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
#define COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace variations {

struct ClientFilterableState;
class VariationsSeedStore;

// The primary class that encapsulates state for managing the safe seed.
class SafeSeedManager {
 public:
  // Creates a SafeSeedManager instance, and updates safe mode prefs for
  // bookkeeping.
  SafeSeedManager(bool did_previous_session_exit_cleanly,
                  PrefService* local_state);
  virtual ~SafeSeedManager();

  // Register safe mode prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns true iff the client should use the safe seed for variations state.
  // Virtual for testing.
  virtual bool ShouldRunInSafeMode() const;

  // Stores the combined server and client state that control the active
  // variations state. Must be called at most once per launch of the Chrome app.
  // As an optimization, should not be called when running in safe mode.
  // Virtual for testing.
  virtual void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time);

  // Records that a fetch has started: pessimistically increments the
  // corresponding failure streak for safe mode.
  void RecordFetchStarted();

  // Records a successful fetch: resets the failure streaks for safe mode.
  // Writes the currently active seed to the |seed_store| as a safe seed, if
  // appropriate.
  void RecordSuccessfulFetch(VariationsSeedStore* seed_store);

 private:
  // The combined server and client state needed to save an active seed as a
  // safe seed. Not set when running in safe mode.
  struct ActiveSeedState {
    ActiveSeedState(
        const std::string& seed_data,
        const std::string& base64_seed_signature,
        std::unique_ptr<ClientFilterableState> client_filterable_state,
        base::Time seed_fetch_time);
    ~ActiveSeedState();

    // The serialized variations seed data.
    const std::string seed_data;

    // The base64-encoded signature for the seed data.
    const std::string base64_seed_signature;

    // The client state which is used for filtering studies.
    const std::unique_ptr<ClientFilterableState> client_filterable_state;

    // The latest timestamp at which this seed was fetched. This is always a
    // client-side timestamp, never a server-provided timestamp.
    const base::Time seed_fetch_time;
  };
  std::unique_ptr<ActiveSeedState> active_seed_state_;

  // The active seed state must never be set more than once.
  bool has_set_active_seed_state_ = false;

  // The pref service used to store persist the variations seed. Weak reference;
  // must outlive |this| instance.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeSeedManager);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
