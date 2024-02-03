// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_
#define COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_

#include "base/component_export.h"
#include "components/version_info/channel.h"

namespace variations {

// Returns true iff the given |channel| is eligible to randomize field trials
// within a layer with LIMITED entropy mode (aka limited layer), or if the
// client has called EnableLimitedEntropyModeForTesting().
COMPONENT_EXPORT(VARIATIONS)
bool IsLimitedEntropyModeEnabled(version_info::Channel channel);

// Enables the client to randomize field trials within a limited layer. For
// testing purposes only.
COMPONENT_EXPORT(VARIATIONS) void EnableLimitedEntropyModeForTesting();

// Disables the client to randomize field trials within a limited layer. For
// testing purposes only.
COMPONENT_EXPORT(VARIATIONS) void DisableLimitedEntropyModeForTesting();

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_LIMITED_ENTROPY_MODE_GATE_H_
