// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/limited_entropy_mode_gate.h"

namespace variations {

namespace {
bool g_is_limited_entropy_mode_enabled_for_testing = false;
}

bool IsLimitedEntropyModeEnabled() {
  if (g_is_limited_entropy_mode_enabled_for_testing) {
    return true;
  }
  // TODO(crbug.com/1511779): Enable limited entropy mode by channel.
  return false;
}

void EnableLimitedEntropyModeForTesting() {
  g_is_limited_entropy_mode_enabled_for_testing = true;
}

}  // namespace variations
