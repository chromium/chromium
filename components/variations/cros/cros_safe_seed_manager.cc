// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros/cros_safe_seed_manager.h"

#include "components/variations/service/safe_seed_manager_interface.h"

namespace variations {
namespace cros_early_boot {

CrOSSafeSeedManager::CrOSSafeSeedManager(SeedType seed) : seed_(seed) {}

CrOSSafeSeedManager::~CrOSSafeSeedManager() = default;

SeedType CrOSSafeSeedManager::GetSeedType() const {
  return seed_;
}

}  // namespace cros_early_boot
}  // namespace variations
