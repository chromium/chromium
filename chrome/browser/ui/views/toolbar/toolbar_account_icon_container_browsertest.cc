// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"

// The param is whether to use the highlight in the container.
class ToolbarAccountIconContainerViewBrowserTest : public InProcessBrowserTest {
 public:
  ToolbarAccountIconContainerViewBrowserTest() {}
  ToolbarAccountIconContainerViewBrowserTest(
      const ToolbarAccountIconContainerViewBrowserTest&) = delete;
  ToolbarAccountIconContainerViewBrowserTest& operator=(
      const ToolbarAccountIconContainerViewBrowserTest&) = delete;
  ~ToolbarAccountIconContainerViewBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableToolbarStatusChip);
    InProcessBrowserTest::SetUp();
  }

  void TestUsesHighlight(ToolbarAccountIconContainerView* container,
                         bool expect_highlight) {
    DCHECK(container);

    // Make sure the save-card icon is visible so that at least two children are
    // visible. Otherwise the border highlight would never be drawn.
    PageActionIconView* save_card_icon =
        container->page_action_icon_controller()->GetIconView(
            PageActionIconType::kSaveCard);
    save_card_icon->SetVisible(true);
    container->Layout();

    EXPECT_EQ(container->uses_highlight(), expect_highlight);
    EXPECT_FALSE(IsHighlighted(container));

    save_card_icon->SetHighlighted(true);
    EXPECT_EQ(IsHighlighted(container), expect_highlight);

    save_card_icon->SetHighlighted(false);
    EXPECT_FALSE(IsHighlighted(container));
  }

  bool IsHighlighted(ToolbarAccountIconContainerView* container) {
    return container->border_.layer()->GetTargetOpacity() == 1;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

IN_PROC_BROWSER_TEST_F(ToolbarAccountIconContainerViewBrowserTest,
                       ShouldUpdateHighlightInNormalWindow) {
  ToolbarAccountIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->toolbar_account_icon_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/true);
}

IN_PROC_BROWSER_TEST_F(ToolbarAccountIconContainerViewBrowserTest,
                       ShouldUpdateHighlightInGuestWindow) {
  Browser* guest_browser = InProcessBrowserTest::CreateGuestBrowser();
  ToolbarAccountIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(guest_browser)
          ->toolbar()
          ->toolbar_account_icon_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/true);
}

IN_PROC_BROWSER_TEST_F(ToolbarAccountIconContainerViewBrowserTest,
                       ShouldNotUpdateHighlightInIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ToolbarAccountIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser)
          ->toolbar()
          ->toolbar_account_icon_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/false);
}
