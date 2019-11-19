// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_CONDITIONAL_SKIPPING_H_
#define CHROME_BROWSER_VR_TEST_CONDITIONAL_SKIPPING_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// A set of macros and functions for conditinoally skipping XR browser tests
// based on available hardware and software.

namespace vr {

enum class XrTestRequirement {
  DIRECTX_11_1,  // Only supported on machines with GPUs that support DX 11.1
};

std::string CheckXrRequirements(
    const std::vector<XrTestRequirement>& requirements_vector,
    const std::unordered_set<std::string>& ignored_set);

std::string XrTestRequirementToString(XrTestRequirement requirement);

inline bool CheckXrRequirementsHelper(
    const std::vector<XrTestRequirement>& requirements_vector,
    const std::unordered_set<std::string>& ignored_set,
    bool* setup_skipped) {
  auto failure_message = CheckXrRequirements(requirements_vector, ignored_set);
  if (failure_message != "") {
    // Newlines to help the skip message stand out in the log.
    LOG(WARNING) << "\n\nSkipping test due to reason: " << failure_message
                 << "\n";
    if (setup_skipped) {
      *setup_skipped = true;
    }
    return true;
  }
  return false;
}

}  // namespace vr

// We use a macro instead of a function because we want to call GTEST_SKIP from
// either the test setup or test implementation. GTEST_SKIP aborts the current
// function only, meaning that if we call it in some sub function, the test will
// continue as normal and only be marked as skipped once it finishes.
#define XR_CONDITIONAL_SKIP(requirements_vector, ignored_set) \
  XR_CONDITIONAL_SKIP_INTERNAL_(requirements_vector, ignored_set, nullptr)

// A special version only meant to be used during the browser test's SetUp
// function since we need to store whether we're skipping before we actually
// do so to work around browser tests not handling skipping in SetUp well due
// to internal checks.
#define XR_CONDITIONAL_SKIP_PRETEST(requirements_vector, ignored_set, \
                                    setup_skipped)                    \
  XR_CONDITIONAL_SKIP_INTERNAL_(requirements_vector, ignored_set, setup_skipped)

#define XR_CONDITIONAL_SKIP_INTERNAL_(requirements_vector, ignored_set, \
                                      setup_skipped)                    \
  if (CheckXrRequirementsHelper(requirements_vector, ignored_set,       \
                                setup_skipped)) {                       \
    GTEST_SKIP();                                                       \
  }

#endif  // CHROME_BROWSER_VR_TEST_CONDITIONAL_SKIPPING_H_
