// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"

class ProductSpecificationsTest : public WebUIMochaBrowserTest {
 protected:
  ProductSpecificationsTest()
      : prefs_(std::make_unique<TestingPrefServiceSimple>()),
        account_checker_(std::make_unique<commerce::MockAccountChecker>()) {
    account_checker_->SetCountry("US");
    account_checker_->SetLocale("en-us");
    account_checker_->SetSignedIn(true);
    account_checker_->SetPrefs(prefs_.get());

    commerce::RegisterCommercePrefs(prefs_->registry());
    commerce::SetTabCompareEnterprisePolicyPref(prefs_.get(), 0);

    set_test_loader_host(commerce::kChromeUICompareHost);
    scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications},
                                          {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProductSpecificationsTest::OnWillCreateBrowserContextServices,
                weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](commerce::MockAccountChecker* checker,
               content::BrowserContext* context) {
              std::unique_ptr<KeyedService> service =
                  commerce::MockShoppingService::Build();
              static_cast<commerce::MockShoppingService*>(service.get())
                  ->SetAccountChecker(checker);
              return service;
            },
            account_checker_.get()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<commerce::MockAccountChecker> account_checker_;
  base::WeakPtrFactory<ProductSpecificationsTest> weak_ptr_factory_{this};
};

// TODO(crbug.com/364441518): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, DISABLED_App) {
  RunTest("commerce/product_specifications/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, BuyingOptionsSection) {
  RunTest("commerce/product_specifications/buying_options_section_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, DescriptionCitation) {
  RunTest("commerce/product_specifications/description_citation_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, DescriptionSection) {
  RunTest("commerce/product_specifications/description_section_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, DisclosureApp) {
  RunTest("commerce/product_specifications/disclosure_app_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, DragAndDropManager) {
  RunTest("commerce/product_specifications/drag_and_drop_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, Header) {
  RunTest("commerce/product_specifications/header_test.js", "mocha.run()");
}

// TODO(crbug.com/370252258): Flaky on win11-arm64-rel-tests.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HorizontalCarousel DISABLED_HorizontalCarousel
#else
#define MAYBE_HorizontalCarousel HorizontalCarousel
#endif
IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, MAYBE_HorizontalCarousel) {
  RunTest("commerce/product_specifications/horizontal_carousel_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, LoadingState) {
  RunTest("commerce/product_specifications/loading_state_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, Table) {
  RunTest("commerce/product_specifications/table_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, NewColumnSelector) {
  RunTest("commerce/product_specifications/new_column_selector_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, ProductSelector) {
  RunTest("commerce/product_specifications/product_selector_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, ProductSelectionMenu) {
  RunTest("commerce/product_specifications/product_selection_menu_test.js",
          "mocha.run()");
}
