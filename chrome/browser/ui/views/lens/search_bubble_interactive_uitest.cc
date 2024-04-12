// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/lens/search_bubble_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
class SearchBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SearchBubbleInteractiveUiTest() = default;
  ~SearchBubbleInteractiveUiTest() override = default;
  SearchBubbleInteractiveUiTest(const SearchBubbleInteractiveUiTest&) = delete;
  void operator=(const SearchBubbleInteractiveUiTest&) = delete;

  auto* GetBubble() {
    auto* controller = lens::SearchBubbleController::FromBrowser(browser());
    return controller->bubble_view_for_testing();
  }

  auto ShowBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      auto* controller =
          lens::SearchBubbleController::GetOrCreateForBrowser(browser());
      controller->Show();
      // Bubble is created synchronously.
      EXPECT_TRUE(!!GetBubble());
    }));
  }

  auto CloseBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      auto* controller = lens::SearchBubbleController::FromBrowser(browser());
      controller->Close();
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
};

IN_PROC_BROWSER_TEST_F(SearchBubbleInteractiveUiTest, BubbleCanShowAndClose) {
  RunTestSequence(EnsureNotPresent(kLensSearchBubbleElementId), ShowBubble(),
                  WaitForShow(kLensSearchBubbleElementId), FlushEvents(),
                  CloseBubble(), WaitForHide(kLensSearchBubbleElementId));
}
}  // namespace lens
