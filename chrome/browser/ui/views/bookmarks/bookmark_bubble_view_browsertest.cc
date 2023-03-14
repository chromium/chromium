// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkBubbleViewBrowserTest() {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
  }

  BookmarkBubbleViewBrowserTest(const BookmarkBubbleViewBrowserTest&) = delete;
  BookmarkBubbleViewBrowserTest& operator=(
      const BookmarkBubbleViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    auto* helper = commerce::ShoppingListUiTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());

    // Clear the original shopping service before we replace it with the new one
    // so we're not dealing with dangling pointers on destruction (of both the
    // service itself and its observers).
    helper->SetShoppingServiceForTesting(nullptr);

    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating([](content::BrowserContext* context) {
                  return commerce::MockShoppingService::Build();
                })));

    helper->SetShoppingServiceForTesting(mock_shopping_service_);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());

    signin::ConsentLevel consent_level = (name == "bookmark_details_synced_off")
                                             ? signin::ConsentLevel::kSignin
                                             : signin::ConsentLevel::kSync;
    constexpr char kTestUserEmail[] = "testuser@gtest.com";
    signin::MakePrimaryAccountAvailable(identity_manager, kTestUserEmail,
                                        consent_level);
#endif

    if (name == "bookmark_details_on_trackable_product") {
      mock_shopping_service_->SetResponseForGetProductInfoForUrl(
          commerce::ProductInfo());
      mock_shopping_service_->SetIsSubscribedCallbackValue(false);
      MockShoppingListUiTabHelper::CreateForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
      MockShoppingListUiTabHelper* mock_tab_helper =
          static_cast<MockShoppingListUiTabHelper*>(
              MockShoppingListUiTabHelper::FromWebContents(
                  browser()->tab_strip_model()->GetActiveWebContents()));
      EXPECT_CALL(*mock_tab_helper, GetProductImage);
      ON_CALL(*mock_tab_helper, GetProductImage)
          .WillByDefault(
              testing::ReturnRef(mock_tab_helper->GetValidProductImage()));
    }

    const GURL url = GURL("https://www.google.com");
    const std::u16string title = u"Title";
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url, title);
    browser()->window()->ShowBookmarkBubble(url, true);

    if (name == "ios_promotion") {
      BookmarkBubbleView::bookmark_bubble()->AcceptDialog();
    }
  }

 private:
  raw_ptr<commerce::MockShoppingService, DanglingUntriaged>
      mock_shopping_service_;
  base::test::ScopedFeatureList test_features_;
};

// Ash always has sync ON
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_synced_off) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_synced_on) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_on_trackable_product) {
  ShowAndVerifyUi();
}
