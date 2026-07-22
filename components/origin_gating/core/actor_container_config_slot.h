// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_SLOT_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_SLOT_H_

#include <optional>

#include "base/types/optional_ref.h"
#include "components/origin_gating/core/actor_container_config.h"

namespace origin_gating {

// A slot that optionally holds an ActorContainerConfig.
class ActorContainerConfigSlot {
 public:
  ActorContainerConfigSlot();
  ActorContainerConfigSlot(const ActorContainerConfigSlot&) = delete;
  ActorContainerConfigSlot& operator=(const ActorContainerConfigSlot&) = delete;
  ActorContainerConfigSlot(ActorContainerConfigSlot&&) = delete;
  ActorContainerConfigSlot& operator=(ActorContainerConfigSlot&&) = delete;
  ~ActorContainerConfigSlot();

  // Assigns the `config` to this instance. This method is a no-op except for
  // the first time it is called.
  void Assign(ActorContainerConfig config);

  bool has_value() const { return config_.has_value(); }

  const ActorContainerConfig& value() const { return config_.value(); }

 private:
  std::optional<ActorContainerConfig> config_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_SLOT_H_
