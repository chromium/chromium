// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_BASE_H_
#define COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"

// This file contains a base class for SafeSeedManager and CrOSSafeSeedManager.
// Its primary goal is to provide a safe seed manager that is generic across
// multiple platforms' notion of when to use a safe seed. A secondary goal of
// this file is to minimize generated code size by reducing dependencies in
// CrOSSafeSeedManager.

namespace variations {

struct ClientFilterableState;
class VariationsSeedStore;

enum class SeedType {
  kRegularSeed,
  kSafeSeed,
  kNullSeed,
};

// The base class that encapsulates state for managing the safe seed.
class SafeSeedManagerBase {
 public:
  SafeSeedManagerBase();

  SafeSeedManagerBase(const SafeSeedManagerBase&) = delete;
  SafeSeedManagerBase& operator=(const SafeSeedManagerBase&) = delete;

  virtual ~SafeSeedManagerBase();

  virtual SeedType GetSeedType() const = 0;

  // Stores the combined server and client state that control the active
  // variations state. May be called at most once per Chrome app launch. As an
  // optimization, should not be called when running in safe mode.
  //
  // Virtual for testing.
  virtual void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      int seed_milestone,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time);

  virtual void RecordFetchStarted() = 0;
  virtual void RecordSuccessfulFetch(VariationsSeedStore* seed_store) = 0;

 protected:
  // The combined server and client state needed to save an active seed as a
  // safe seed. Not set when running in safe mode.
  struct ActiveSeedState {
    ActiveSeedState(
        const std::string& seed_data,
        const std::string& base64_seed_signature,
        int seed_milestone,
        std::unique_ptr<ClientFilterableState> client_filterable_state,
        base::Time seed_fetch_time);

    ~ActiveSeedState();

    // The serialized variations seed data.
    const std::string seed_data;

    // The base64-encoded signature for the seed data.
    const std::string base64_seed_signature;

    // The milestone with which the active seed was fetched.
    const int seed_milestone;

    // The client state which is used for filtering studies.
    const std::unique_ptr<ClientFilterableState> client_filterable_state;

    // The latest timestamp at which this seed was fetched. This is always a
    // client-side timestamp, never a server-provided timestamp.
    const base::Time seed_fetch_time;
  };

  // Accessor for active_seed_state_.
  const std::optional<ActiveSeedState>& GetActiveSeedState() const;

  // Resets active_seed_state_;
  void ClearActiveSeedState();

 private:
  std::optional<ActiveSeedState> active_seed_state_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_BASE_H_
