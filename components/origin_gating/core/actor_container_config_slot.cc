// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config_slot.h"

#include <utility>

#include "base/types/optional_ref.h"
#include "components/origin_gating/core/actor_container_config.h"

namespace origin_gating {

ActorContainerConfigSlot::ActorContainerConfigSlot() = default;
ActorContainerConfigSlot::~ActorContainerConfigSlot() = default;

bool ActorContainerConfigSlot::Assign(ActorContainerConfig config) {
  if (config_.has_value()) {
    return false;
  }
  config_.emplace(std::move(config));
  return true;
}

}  // namespace origin_gating
