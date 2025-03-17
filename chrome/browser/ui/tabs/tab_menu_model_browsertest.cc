// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feed/web_feed_tab_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"

class TabMenuModelBrowserTest : public MenuModelTest,
                                public InProcessBrowserTest {
 public:
  TabMenuModelBrowserTest() {
    // Enable tab organization before KeyedServices are instantiated, otherwise
    // TabOrganizationServiceFactory::GetForProfile() will return nullptr.
    feature_list_.InitWithFeatures({features::kTabOrganization}, {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  tabs::PreventTabFeatureInitialization prevent_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, Basics) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5u);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate_.execute_count_);
  EXPECT_EQ(item_count, delegate_.enable_count_);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, OrganizeTabs) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify that CommandOrganizeTabs is in the menu.
  EXPECT_TRUE(model.GetIndexOfCommandId(TabStripModel::CommandOrganizeTabs)
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, MoveToNewWindow) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify that CommandMoveTabsToNewWindow is in the menu.
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, AddToExistingGroupSubmenu) {
  // Prevents flakes by ensuring the TabGroupSyncService is initialized before
  // creating any tab groups.
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile());
  service->SetIsInitializedForTesting(true);

  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 3);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 5u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
  EXPECT_EQ(submenu->GetCommandIdAt(4),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 3);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest,
                       AddToExistingGroupSubmenu_DoesNotIncludeCurrentGroup) {
  // Prevents flakes by ensuring the TabGroupSyncService is initialized before
  // creating any tab groups.
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile());
  service->SetIsInitializedForTesting(true);

  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 4u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
}

// In some cases, groups may change after the menu is created. For example an
// extension may modify groups while the menu is open. If a group referenced in
// the menu goes away, ensure we handle this gracefully.
//
// Regression test for crbug.com/1197875
IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest,
                       AddToExistingGroupAfterGroupDestroyed) {
  // Prevents flakes by ensuring the TabGroupSyncService is initialized before
  // creating any tab groups.
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile());
  service->SetIsInitializedForTesting(true);

  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({0});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 3u);

  // Ungroup the tab at 0 to make the group in the menu dangle.
  tab_strip_model->RemoveFromGroup({0});

  // Try adding to the group from the menu.
  submenu->ActivatedAt(2);

  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1).has_value());
}

class TabMenuModelTestTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  bool IsForWebApp() override { return true; }

  bool SupportsReadLater() override { return false; }
};

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, TabbedWebApp) {
  // Create a tabbed web app window without home tab
  TabMenuModelTestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      true);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     &tab_strip_model, 0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(model.GetItemCount(), 7u);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCopyURL).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandReload).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandGoBack).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCloseTab).has_value());
  EXPECT_TRUE(model.GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, TabbedWebAppHomeTab) {
  TabMenuModelTestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      true);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);
  // Pin the first tab so we get the pinned home tab menu.
  tab_strip_model.SetTabPinned(0, true);

  TabMenuModel home_tab_model(&delegate_, browser()->tab_menu_model_delegate(),
                              &tab_strip_model, 0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(home_tab_model.GetItemCount(), 5u);

  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
                  .has_value());

  EXPECT_EQ(home_tab_model.GetTypeAt(3), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      true);
  EXPECT_EQ(tab_strip_model.count(), 2);
  EXPECT_FALSE(tab_strip_model.IsTabSelected(0));
  EXPECT_TRUE(tab_strip_model.IsTabSelected(1));

  TabMenuModel regular_tab_model(
      &delegate_, browser()->tab_menu_model_delegate(), &tab_strip_model, 1);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(regular_tab_model.GetItemCount(), 8u);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model
          .GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(regular_tab_model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseTab)
          .has_value());
  EXPECT_TRUE(regular_tab_model
                  .GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());
}

class TabMenuModelCommerceProductSpecsTest : public TabMenuModelBrowserTest {
 public:
  TabMenuModelCommerceProductSpecsTest()
      : account_checker_(std::make_unique<commerce::MockAccountChecker>()),
        prefs_(std::make_unique<TestingPrefServiceSimple>()) {
    feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});

    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabMenuModelCommerceProductSpecsTest::SetTestingFactory,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    TabMenuModelBrowserTest::SetUpOnMainThread();
    commerce::MockAccountChecker::RegisterCommercePrefs(prefs_->registry());
    account_checker_->SetPrefs(prefs_.get());
    auto* shopping_service = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
    shopping_service->SetAccountChecker(account_checker_.get());
    // By default, the account checker and prefs are set up to enable product
    // specifications.
    commerce::EnableProductSpecificationsDataFetch(account_checker_.get(),
                                                   prefs_.get());
  }

  void SetTestingFactory(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return commerce::MockShoppingService::Build();
        }));
  }

 protected:
  std::unique_ptr<commerce::MockAccountChecker> account_checker_;

 private:
  base::CallbackListSubscription dependency_manager_subscription_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest,
                       MenuShowForNormalWindow) {
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example2.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip->AddSelectionFromAnchorTo(1);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);
  EXPECT_TRUE(model
                  .GetIndexOfCommandId(
                      TabStripModel::CommandCommerceProductSpecifications)
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest,
                       MenuNotShowForIncognitoWindow) {
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  Browser* incognito_browser = CreateIncognitoBrowser(profile());

  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, GURL("https://example.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, GURL("https://example2.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close about:blank tab since we don't need it.
  incognito_browser->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_NONE);

  TabStripModel* tab_strip = incognito_browser->tab_strip_model();
  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip->AddSelectionFromAnchorTo(1);

  TabMenuModel model(&delegate_, incognito_browser->tab_menu_model_delegate(),
                     incognito_browser->tab_strip_model(), 0);
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandCommerceProductSpecifications)
                   .has_value());

  // All tabs must be closed before the object is destroyed.
  incognito_browser->tab_strip_model()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest,
                       MenuNotShowForInvalidScheme) {
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://bookmarks"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://history"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip->AddSelectionFromAnchorTo(1);
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandCommerceProductSpecifications)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest, MenuShowForHttp) {
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example2.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip->AddSelectionFromAnchorTo(1);
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  EXPECT_TRUE(model
                  .GetIndexOfCommandId(
                      TabStripModel::CommandCommerceProductSpecifications)
                  .has_value());
}

class TabMenuModelCommerceProductSpecsDisabledTest
    : public TabMenuModelCommerceProductSpecsTest {
 public:
  TabMenuModelCommerceProductSpecsDisabledTest() {
    feature_list_.InitWithFeatures({}, {commerce::kProductSpecifications});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsDisabledTest,
                       MenuNotShowForFeatureDisable) {
  ASSERT_FALSE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  chrome::NewTab(browser());

  tab_strip->AddSelectionFromAnchorTo(1);
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandCommerceProductSpecifications)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest,
                       MenuNotShowForFetchDisable) {
  // Update account checker to disable product specifications data fetch.
  account_checker_->SetIsSubjectToParentalControls(true);
  ASSERT_FALSE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  tab_strip->AddSelectionFromAnchorTo(1);
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandCommerceProductSpecifications)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelCommerceProductSpecsTest,
                       MenuNotShowForInsuffcientSelection) {
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandCommerceProductSpecifications)
                   .has_value());
}

class TabMenuModelComparisonTableTest : public TabMenuModelBrowserTest {
 public:
  TabMenuModelComparisonTableTest() {
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabMenuModelComparisonTableTest::SetTestingFactory,
                base::Unretained(this)));

    feature_list_.InitWithFeatures({commerce::kProductSpecifications,
                                    commerce::kCompareManagementInterface},
                                   {});
  }

  void SetTestingFactory(content::BrowserContext* context) {
    commerce::ProductSpecificationsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              return commerce::MockProductSpecificationsService::Build();
            }));
  }

 protected:
  TabStripModel* tab_strip() { return browser()->tab_strip_model(); }

  void AddAndSelectTab(Browser* browser, const GURL& url) {
    // ASSERT_TRUE(
    // AddTabAtIndex(0, url, ui::PageTransition::PAGE_TRANSITION_TYPED));
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    browser->tab_strip_model()->ToggleSelectionAt(
        browser->tab_strip_model()->count() - 1);
  }

  void SelectAllTabs() {
    tab_strip()->AddSelectionFromAnchorTo(tab_strip()->count() - 1);
  }

  void SetProductSpecs(
      const std::vector<commerce::ProductSpecificationsSet>& sets) {
    auto* product_specs_service =
        static_cast<commerce::MockProductSpecificationsService*>(
            commerce::ProductSpecificationsServiceFactory::GetForBrowserContext(
                browser()->profile()));
    ON_CALL(*product_specs_service, GetAllProductSpecifications())
        .WillByDefault(testing::Return(sets));
  }

  base::CallbackListSubscription dependency_manager_subscription_;
  base::test::ScopedFeatureList feature_list_;
};

class TabMenuModelComparisonTableDisabledTest
    : public TabMenuModelComparisonTableTest {
 public:
  TabMenuModelComparisonTableDisabledTest() {
    feature_list_.InitWithFeatures({}, {commerce::kProductSpecifications,
                                        commerce::kCompareManagementInterface});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableDisabledTest,
                       MenuNotShownWhenFeatureDisabled) {
  AddAndSelectTab(browser(), GURL("https://example.com"));

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     tab_strip(), 0);
  EXPECT_FALSE(
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable)
          .has_value());
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandAddToExistingComparisonTable)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableTest,
                       MenuShownForNormalWindow) {
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     tab_strip(), 0);

  // No existing tables, so only the option for adding to a new table should be
  // visible.
  auto index =
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable);
  ASSERT_TRUE(index.has_value());
  EXPECT_TRUE(model.IsEnabledAt(index.value()));
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandAddToExistingComparisonTable)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableTest,
                       MenuNotShownForIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser(profile());

  AddAndSelectTab(incognito_browser, GURL("https://example.com"));

  TabMenuModel model(&delegate_, incognito_browser->tab_menu_model_delegate(),
                     incognito_browser->tab_strip_model(), 0);
  EXPECT_FALSE(
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable)
          .has_value());
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandAddToExistingComparisonTable)
                   .has_value());

  // All tabs must be closed before the object is destroyed.
  incognito_browser->tab_strip_model()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableTest,
                       MenuNotShownWhenMultipleTabsSelected) {
  AddAndSelectTab(browser(), GURL("https://example.com"));
  AddAndSelectTab(browser(), GURL("https://sample.com"));

  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  SelectAllTabs();

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     tab_strip(), 0);

  EXPECT_FALSE(
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable)
          .has_value());
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandAddToExistingComparisonTable)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableTest,
                       MenuShownForExistingTables_SetsDoNotContainUrl) {
  const std::vector<commerce::ProductSpecificationsSet> sets = {
      commerce::ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
          {
              GURL("https://example1.com"),
          },
          "Set 1"),
      commerce::ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
          {
              GURL("https://example2.com"),
          },
          "Set 2")};
  SetProductSpecs(sets);

  AddAndSelectTab(browser(), GURL("https://example.com"));
  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     tab_strip(), 0);

  // There are existing tables, so the submenu for adding to an existing table
  // should be visible.
  EXPECT_FALSE(
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable)
          .has_value());
  auto index = model.GetIndexOfCommandId(
      TabStripModel::CommandAddToExistingComparisonTable);
  EXPECT_TRUE(index.has_value());
  EXPECT_TRUE(model.IsEnabledAt(index.value()));
}

IN_PROC_BROWSER_TEST_F(TabMenuModelComparisonTableTest,
                       MenuShownForExistingTables_SetsContainUrl) {
  const std::vector<commerce::ProductSpecificationsSet> sets = {
      commerce::ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
          {
              GURL("https://example.com"),
          },
          "Set 1"),
      commerce::ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
          {
              GURL("https://example.com"),
          },
          "Set 2")};
  SetProductSpecs(sets);

  AddAndSelectTab(browser(), GURL("https://example.com"));
  // Close about:blank tab since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     tab_strip(), 0);

  // All existing tables contain the URL, so only the option for adding to a new
  // table should be visible.
  auto index =
      model.GetIndexOfCommandId(TabStripModel::CommandAddToNewComparisonTable);
  EXPECT_TRUE(index.has_value());
  EXPECT_TRUE(model.IsEnabledAt(index.value()));
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       TabStripModel::CommandAddToExistingComparisonTable)
                   .has_value());
}
