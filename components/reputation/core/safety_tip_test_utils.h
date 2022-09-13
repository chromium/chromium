// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPUTATION_CORE_SAFETY_TIP_TEST_UTILS_H_
#define COMPONENTS_REPUTATION_CORE_SAFETY_TIP_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/reputation/core/safety_tips.pb.h"

namespace reputation {

// Retrieve any existing Safety Tips config proto if set, or create a new one
// otherwise.
std::unique_ptr<SafetyTipsConfig> GetOrCreateSafetyTipsConfig();

// Initialize component configuration. Necessary to enable Safety Tips for
// testing, as no heuristics trigger if the allowlist is inaccessible.
void InitializeSafetyTipConfig();

// Sets the patterns included in component with the given flag type for tests.
// This will replace any flag patterns currently in the proto.
void SetSafetyTipPatternsWithFlagType(std::vector<std::string> pattern,
                                      FlaggedPage::FlagType type);

// Sets the patterns to trigger a bad-reputation Safety Tip for tests. This just
// calls SetSafetyTipPatternsWithFlagType with BAD_REPUTATION as the type.
void SetSafetyTipBadRepPatterns(std::vector<std::string> pattern);

// Sets allowlist patterns in the given proto for testing. This will replace any
// allowlist patterns currently in the proto.
// |patterns| is the list of hostnames allowed to be lookalikes.
// |target_patterns| is the list of hostname regexes allowed to be targets of
// lookalikes.
void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns,
                                   std::vector<std::string> target_patterns,
                                   std::vector<std::string> common_words);

// Adds a launch config for the given heuristic with the given percentage. See
// the proto definition for the meaning of various launch percentage values.
void AddSafetyTipHeuristicLaunchConfigForTesting(
    reputation::HeuristicLaunchConfig::Heuristic heuristic,
    int launch_percentage);

// Ensure that the allowlist has been initialized. This is important as some
// code (e.g. the elision policy) is fail-open (i.e. it won't elide without an
// initialized allowlist). This is convenience wrapper around
// SetSafetyTipAllowlistPatterns().
void InitializeBlankLookalikeAllowlistForTesting();

}  // namespace reputation

#endif  // COMPONENTS_REPUTATION_CORE_SAFETY_TIP_TEST_UTILS_H_
