// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_controller.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"

namespace {

class RollBackModeBInfoBarControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
    controller_ =
        std::make_unique<RollBackModeBInfoBarController>(web_contents());
  }

  void Navigate(bool has_committed = true, bool primary_main_frame = true) {
    content::MockNavigationHandle navigation_handle(GURL(), main_rfh());
    navigation_handle.set_has_committed(has_committed);
    navigation_handle.set_is_in_primary_main_frame(primary_main_frame);
    controller()->DidFinishNavigation(&navigation_handle);
  }

  RollBackModeBInfoBarController* controller() { return controller_.get(); }

 private:
  ChromeLayoutProvider layout_provider_;
  std::unique_ptr<RollBackModeBInfoBarController> controller_;
};

TEST_F(RollBackModeBInfoBarControllerTest, DoesNotAddInfobarWhenPrefIsFalse) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB, false);
  Navigate();

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  EXPECT_TRUE(infobar_manager->infobars().empty());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowRollbackUiModeB));
}

TEST_F(RollBackModeBInfoBarControllerTest, ShowsAndHidesInfobarAndSetsPref) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB, true);
  Navigate();

  // Verify infobar was added and pref was set.
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  EXPECT_EQ(infobar_manager->infobars()[0]->GetIdentifier(),
            infobars::InfoBarDelegate::ROLL_BACK_MODE_B_INFOBAR_DELEGATE);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowRollbackUiModeB));

  // Visibility change to `VISIBLE` should not hide infobar.
  controller()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  EXPECT_EQ(infobar_manager->infobars()[0]->GetIdentifier(),
            infobars::InfoBarDelegate::ROLL_BACK_MODE_B_INFOBAR_DELEGATE);

  // Visibility change to `HIDDEN` should hide infobar.
  controller()->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

TEST_F(RollBackModeBInfoBarControllerTest,
       DoesNotAddInfobarWhenNavigationNotCommitted) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB, true);
  Navigate(/*has_committed=*/false);

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  EXPECT_TRUE(infobar_manager->infobars().empty());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowRollbackUiModeB));
}

TEST_F(RollBackModeBInfoBarControllerTest,
       DoesNotAddInfobarWhenNavigationNotInPrimaryMainFrame) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB, true);
  Navigate(/*has_committed=*/true, /*primary_main_frame=*/false);

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  EXPECT_TRUE(infobar_manager->infobars().empty());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowRollbackUiModeB));
}

}  // namespace
