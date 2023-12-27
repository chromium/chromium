// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_
#define COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_

#include "base/component_export.h"

namespace variations {

// Returns true iff this client is eligible to randomize field trials within a
// layer with LIMITED entropy mode, or if the client has called
// EnableLimitedEntropyModeForTesting().
COMPONENT_EXPORT(VARIATIONS) bool IsLimitedEntropyModeEnabled();

// Enables the client to randomize field trials within a layer with LIMITED
// entropy mode. For testing purposes only.
COMPONENT_EXPORT(VARIATIONS) void EnableLimitedEntropyModeForTesting();

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_
