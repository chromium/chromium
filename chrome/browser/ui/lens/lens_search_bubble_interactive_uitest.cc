// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
class LensSearchBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  LensSearchBubbleInteractiveUiTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {
                                          {"search-bubble", "true"},
                                      });
  }
  ~LensSearchBubbleInteractiveUiTest() override = default;
  LensSearchBubbleInteractiveUiTest(const LensSearchBubbleInteractiveUiTest&)
  = delete;
  void operator=(const LensSearchBubbleInteractiveUiTest&) = delete;

  auto* GetBubble() {
    auto* controller = lens::LensSearchBubbleController::FromBrowser(browser());
    return controller->bubble_view_for_testing();
  }

  auto ShowBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      auto* controller =
          lens::LensSearchBubbleController::GetOrCreateForBrowser(browser());
      controller->Show();
      // Bubble is created synchronously.
      EXPECT_TRUE(!!GetBubble());
    }));
  }

  auto CloseBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      auto* controller = lens::LensSearchBubbleController::FromBrowser(
                                               browser());
      controller->Close();
      EXPECT_FALSE(!!GetBubble());
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensSearchBubbleInteractiveUiTest,
                       BubbleCanShowAndClose) {
  RunTestSequence(EnsureNotPresent(kLensSearchBubbleElementId), ShowBubble(),
                  WaitForShow(kLensSearchBubbleElementId), FlushEvents(),
                  CloseBubble(), WaitForHide(kLensSearchBubbleElementId));
}
}  // namespace lens
