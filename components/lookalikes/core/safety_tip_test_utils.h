// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_SAFETY_TIP_TEST_UTILS_H_
#define COMPONENTS_LOOKALIKES_CORE_SAFETY_TIP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/lookalikes/core/safety_tips.pb.h"

namespace lookalikes {

// Retrieve any existing Safety Tips config proto if set, or create a new one
// otherwise.
std::unique_ptr<reputation::SafetyTipsConfig> GetOrCreateSafetyTipsConfig();

// Initialize component configuration. Necessary to enable Safety Tips for
// testing, as no heuristics trigger if the allowlist is inaccessible.
void InitializeSafetyTipConfig();

// Sets allowlist patterns in the given proto for testing. This will replace any
// allowlist patterns currently in the proto.
// |patterns| is the list of hostnames allowed to be lookalikes.
// |target_patterns| is the list of hostname regexes allowed to be targets of
// lookalikes.
void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns,
                                   std::vector<std::string> target_patterns,
                                   std::vector<std::string> common_words);

// Ensure that the allowlist has been initialized. This is important as some
// code (e.g. the elision policy) is fail-open (i.e. it won't elide without an
// initialized allowlist). This is convenience wrapper around
// SetSafetyTipAllowlistPatterns().
void InitializeBlankLookalikeAllowlistForTesting();

// Adds a launch config for the given heuristic with the given percentage. See
// the proto definition for the meaning of various launch percentage values.
void AddSafetyTipHeuristicLaunchConfigForTesting(
    reputation::HeuristicLaunchConfig::Heuristic heuristic,
    int launch_percentage);

}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_SAFETY_TIP_TEST_UTILS_H_
