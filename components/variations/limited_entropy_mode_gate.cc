// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/limited_entropy_mode_gate.h"

namespace variations {

namespace {

enum class ForcedState {
  ENABLED,
  DISABLED,
  UNSET,
};

ForcedState g_forced_state_for_testing = ForcedState::UNSET;
}

bool IsLimitedEntropyModeEnabled(version_info::Channel channel) {
  if (g_forced_state_for_testing != ForcedState::UNSET) {
    return g_forced_state_for_testing == ForcedState::ENABLED;
  }
  // Limited entropy mode is enabled in all channels.
  return true;
}

void EnableLimitedEntropyModeForTesting() {
  g_forced_state_for_testing = ForcedState::ENABLED;
}

void DisableLimitedEntropyModeForTesting() {
  g_forced_state_for_testing = ForcedState::DISABLED;
}

}  // namespace variations
