// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/actor/actor_utils.h"

#include "chrome/common/chrome_features.h"

namespace actor {

bool UseGeneralPageStabilityNavigationTools() {
  switch (features::kActorGeneralPageStabilityMode.Get()) {
    case features::ActorGeneralPageStabilityMode::kDisabled:
      return false;
    case features::ActorGeneralPageStabilityMode::kNavigateAndHistoryEnabled:
    case features::ActorGeneralPageStabilityMode::kAllEnabled:
      return true;
  }
}

bool UseGeneralPageStabilityAllTools() {
  switch (features::kActorGeneralPageStabilityMode.Get()) {
    case features::ActorGeneralPageStabilityMode::kDisabled:
    case features::ActorGeneralPageStabilityMode::kNavigateAndHistoryEnabled:
      return false;
    case features::ActorGeneralPageStabilityMode::kAllEnabled:
      return true;
  }
}

}  // namespace actor
