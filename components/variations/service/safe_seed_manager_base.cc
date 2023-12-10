// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager_base.h"

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/variations/client_filterable_state.h"

namespace variations {

SafeSeedManagerBase::SafeSeedManagerBase() = default;

SafeSeedManagerBase::~SafeSeedManagerBase() = default;

void SafeSeedManagerBase::SetActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    int seed_milestone,
    std::unique_ptr<ClientFilterableState> client_filterable_state,
    base::Time seed_fetch_time) {
  DCHECK(!active_seed_state_.has_value());

  active_seed_state_.emplace(seed_data, base64_seed_signature, seed_milestone,
                             std::move(client_filterable_state),
                             seed_fetch_time);
}

SafeSeedManagerBase::ActiveSeedState::ActiveSeedState(
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

SafeSeedManagerBase::ActiveSeedState::~ActiveSeedState() = default;

const std::optional<SafeSeedManagerBase::ActiveSeedState>&
SafeSeedManagerBase::GetActiveSeedState() const {
  return active_seed_state_;
}

void SafeSeedManagerBase::ClearActiveSeedState() {
  active_seed_state_.reset();
}

}  // namespace variations
