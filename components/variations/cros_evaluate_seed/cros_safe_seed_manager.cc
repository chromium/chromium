// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/cros_safe_seed_manager.h"

#include "components/variations/service/safe_seed_manager_interface.h"

namespace variations::cros_early_boot::evaluate_seed {

CrOSSafeSeedManager::CrOSSafeSeedManager(SeedType seed) : seed_(seed) {}

CrOSSafeSeedManager::~CrOSSafeSeedManager() = default;

SeedType CrOSSafeSeedManager::GetSeedType() const {
  return seed_;
}

}  // namespace variations::cros_early_boot::evaluate_seed
