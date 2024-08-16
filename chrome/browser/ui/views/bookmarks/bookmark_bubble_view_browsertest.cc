// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BaseBookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BaseBookmarkBubbleViewBrowserTest() = default;

  BaseBookmarkBubbleViewBrowserTest(const BaseBookmarkBubbleViewBrowserTest&) =
      delete;
  BaseBookmarkBubbleViewBrowserTest& operator=(
      const BaseBookmarkBubbleViewBrowserTest&) = delete;

  ~BaseBookmarkBubbleViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    EXPECT_TRUE(is_browser_context_services_created);
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BaseBookmarkBubbleViewBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
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
      commerce::ProductInfo info;
      info.product_cluster_id.emplace(12345L);
      mock_shopping_service_->SetIsShoppingListEligible(true);
      mock_shopping_service_->SetResponseForGetProductInfoForUrl(info);
      mock_shopping_service_->SetIsSubscribedCallbackValue(false);
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

 protected:
  base::test::ScopedFeatureList test_features_;

 private:
  raw_ptr<commerce::MockShoppingService, DanglingUntriaged>
      mock_shopping_service_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};
  base::WeakPtrFactory<BaseBookmarkBubbleViewBrowserTest> weak_ptr_factory_{
      this};
};

class PowerBookmarkBubbleViewBrowserTest
    : public BaseBookmarkBubbleViewBrowserTest {
 public:
  PowerBookmarkBubbleViewBrowserTest() {
    MockCommerceUiTabHelper::ReplaceFactory();
    test_features_.InitWithFeatures({commerce::kShoppingList}, {});
  }

  PowerBookmarkBubbleViewBrowserTest(
      const PowerBookmarkBubbleViewBrowserTest&) = delete;
  PowerBookmarkBubbleViewBrowserTest& operator=(
      const PowerBookmarkBubbleViewBrowserTest&) = delete;

  ~PowerBookmarkBubbleViewBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PowerBookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_on_trackable_product) {
  ShowAndVerifyUi();
}
