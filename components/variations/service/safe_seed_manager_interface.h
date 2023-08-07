// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_INTERFACE_H_
#define COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_INTERFACE_H_

#include <memory>
#include <string>

#include "base/time/time.h"

// This file contains an interface for SafeSeedManager and CrOSSafeSeedManager.
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

// The interface that encapsulates state for managing the safe seed.
class SafeSeedManagerInterface {
 public:
  virtual ~SafeSeedManagerInterface() = default;

  virtual SeedType GetSeedType() const = 0;
  virtual void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      int seed_milestone,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time) = 0;
  virtual void RecordFetchStarted() = 0;
  virtual void RecordSuccessfulFetch(VariationsSeedStore* seed_store) = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_INTERFACE_H_
