// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

}  // namespace

class PerformanceInterventionInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kPerformanceIntervention);
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Pixel test to verify that the performance intervention toolbar
// button looks correct.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       InterventionToolbarButton) {
  RunTestSequence(SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                          kSkipPixelTestsReason),
                  WaitForShow(kToolbarPerformanceInterventionButtonElementId),
                  Screenshot(kToolbarPerformanceInterventionButtonElementId,
                             /*screenshot_name=*/"InterventionToolbarButton",
                             /*baseline_cl=*/"5503223"));
}
