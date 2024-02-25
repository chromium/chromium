// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_ANNOTATIONS_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_ANNOTATIONS_H_

namespace crosier {

// Annotate Crosier tests with requirements that the test has. Use these
// annotations at the top of your test body or in the SetUp function in
// the TestCase.
//
// • Informational tests:
//
//     Use to indicate that the test is possibly flaky and shouldn't be
//     run in a context where it blocks patches or work. Such tests are
//     not run by default. To run informational tests, pass
//     --informational_tests to the test on the command line.
//
//       IN_PROC_BROWSER_TEST_P(DoomMelonTest, ButtonPress) {
//         TEST_IS_INFORMATIONAL();
//
//         ... test code...
//       }
//
// • Board requirements:
//
//     Use to annotate specific hardware requirements. Example:
//
//       IN_PROC_BROWSER_TEST_P(DoomMelonTest, ButtonPress) {
//         TEST_REQUIRES(crosier::Requirement::kBluetooth);
//
//         ... test code...
//       }
//
//
// The TEST_REQUIRES() and TEST_IS_INFORMATIONAL() macros call the query
// functions crosier::HasRequirement() and
// crosier::ShouldRunInformationalTests() and calls GTEST_SKIP() if it fails.
// This will mark the test skipped and return void from the current function.
//
// Because of the way GTEST_SKIP() works, you can't call it in a
// constructor or a function that returns non-void. If you need to call
// it in other contexts, just use the C++ functions instead of the macros
// and call GTEST_SKIP() manually in the correct context.

enum class Requirement {
  kBluetooth,
  kOndeviceHandwriting,
  kVulkan,
};

bool ShouldRunInformationalTests();
bool HasRequirement(Requirement);

#define TEST_IS_INFORMATIONAL()                                               \
  if (!::crosier::ShouldRunInformationalTests()) {                            \
    GTEST_SKIP() << "Skipping informational test. Use --informational_tests " \
                    "to enable.";                                             \
    return;                                                                   \
  }

#define TEST_REQUIRES(feature)                                                \
  if (!::crosier::HasRequirement(feature)) {                                  \
    GTEST_SKIP() << "Skipping test because target doesn't support " #feature; \
  }

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_ANNOTATIONS_H_
