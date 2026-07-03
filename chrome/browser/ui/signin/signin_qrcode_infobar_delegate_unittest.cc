// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"

#include <memory>
#include <utility>

#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninQRCodeInfoBarDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  SigninQRCodeInfoBarDelegateTest() = default;
  ~SigninQRCodeInfoBarDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    DiceTabHelper::CreateForWebContents(web_contents());
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(web_contents());
  }
};

// Test that the infobar is successfully added and then automatically removed
// when the DiceTabHelper notifies that it is no longer the sign-in page.
TEST_F(SigninQRCodeInfoBarDelegateTest, AddAndRemoveOnStateChange) {
  infobars::ContentInfoBarManager* manager = infobar_manager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0u, manager->infobars().size());

  // Create the delegate and wrap it in an InfoBar.
  auto delegate = std::make_unique<SigninQRCodeInfoBarDelegate>(web_contents());
  auto test_infobar = std::make_unique<infobars::InfoBar>(std::move(delegate));

  // Add the InfoBar to the manager.
  infobars::InfoBar* added_infobar =
      manager->AddInfoBar(std::move(test_infobar));
  ASSERT_TRUE(added_infobar);
  EXPECT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(added_infobar, manager->infobars()[0]);

  // Initialize the sign-in flow on the helper (sets is_chrome_signin_page to
  // true).
  DiceTabHelper::FromWebContents(web_contents())
      ->InitializeSigninFlow(
          GURL("https://accounts.google.com/signin"),
          signin_metrics::AccessPoint::kAvatarBubbleSignIn,
          signin_metrics::Reason::kSigninPrimaryAccount,
          signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
          GURL("https://redirect.com"),
          /*record_signin_started_metrics=*/false, base::DoNothing(),
          base::DoNothing(), base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(
      DiceTabHelper::FromWebContents(web_contents())->IsChromeSigninPage());

  // Navigate away from the sign-in page. This should naturally trigger
  // SetIsChromeSigninPage(false) and remove the infobar.
  NavigateAndCommit(GURL("https://www.google.com"));

  // Verify that the infobar has been removed automatically.
  EXPECT_EQ(0u, manager->infobars().size());
}

// Test that the delegate handles WebContents destruction cleanly without
// triggering dangling pointer crashes or Use-After-Free (UAF).
TEST_F(SigninQRCodeInfoBarDelegateTest, DestructionObservationSafety) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  DiceTabHelper::CreateForWebContents(contents.get());
  infobars::ContentInfoBarManager::CreateForWebContents(contents.get());

  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(contents.get());
  ASSERT_TRUE(manager);

  auto delegate = std::make_unique<SigninQRCodeInfoBarDelegate>(contents.get());
  auto test_infobar = std::make_unique<infobars::InfoBar>(std::move(delegate));

  infobars::InfoBar* added_infobar =
      manager->AddInfoBar(std::move(test_infobar));
  ASSERT_TRUE(added_infobar);
  EXPECT_EQ(1u, manager->infobars().size());

  // Destroy the WebContents. The delegate's WebContentsDestroyed() should run,
  // detaching its observation and resetting pointers cleanly.
  contents.reset();
}
