// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/callback_list.h"
#include "base/json/json_reader.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_test_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include "extensions/common/extension_features.h"
#endif

using testing::_;

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class TabStripModelPreventCloseTest : public PreventCloseTestBase,
                                      public TabStripModelObserver {
 public:
  TabStripModelPreventCloseTest() = default;
  explicit TabStripModelPreventCloseTest(const PreventCloseTestBase&) = delete;
  TabStripModelPreventCloseTest& operator=(
      const TabStripModelPreventCloseTest&) = delete;

  // TabStripModelObserver:
  MOCK_METHOD(void,
              OnTabCloseCancelled,
              (const tabs::TabInterface* tab),
              (override));

 protected:
  void ObserveBrowser(Browser* browser) {
    browser_did_close_subscription_ =
        browser->RegisterBrowserDidClose(base::BindRepeating(
            [](TabStripModelPreventCloseTest* self, BrowserWindowInterface*) {
              self->observer_.Reset();
            },
            base::Unretained(this)));
    observer_.Observe(browser->tab_strip_model());
  }

  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::ScopedObservation<TabStripModel, TabStripModelPreventCloseTest>
      observer_{this};
  base::CallbackListSubscription browser_did_close_subscription_;
};

IN_PROC_BROWSER_TEST_F(TabStripModelPreventCloseTest,
                       PreventCloseEnforedByPolicy) {
  InstallPWA(GURL(kCalculatorAppUrl), ash::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(ash::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);
  Browser* const browser =
      LaunchPWA(ash::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  ObserveBrowser(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(!kShouldPreventClose, tab_strip_model->IsTabClosable(
                                      tab_strip_model->GetActiveWebContents()));

  EXPECT_CALL(*this, OnTabCloseCancelled(_)).Times(kShouldPreventClose ? 1 : 0);

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(kShouldPreventClose ? 1 : 0, tab_strip_model->count());

  if (kShouldPreventClose) {
    ClearWebAppSettings();
    EXPECT_TRUE(tab_strip_model->IsTabClosable(
        tab_strip_model->GetActiveWebContents()));

    tab_strip_model->CloseAllTabs();
    EXPECT_EQ(0, tab_strip_model->count());
  }
}

// TODO(b/321593065): enable this flaky test.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  DISABLED_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#else
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#endif
IN_PROC_BROWSER_TEST_F(
    TabStripModelPreventCloseTest,
    MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable) {
  InstallPWA(GURL(kCalculatorAppUrl), ash::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(ash::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);
  Browser* const browser =
      LaunchPWA(ash::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  ObserveBrowser(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_NE(0, tab_strip_model->count());
  EXPECT_TRUE(
      tab_strip_model->IsTabClosable(tab_strip_model->GetActiveWebContents()));

  EXPECT_CALL(*this, OnTabCloseCancelled(_)).Times(0);

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(0, tab_strip_model->count());
}

class TabStripModelBrowserTest : public InProcessBrowserTest,
                                 public TabStripModelObserver {
 public:
  void TearDownOnMainThread() override { observer_.Reset(); }

  MOCK_METHOD(void,
              OnTabGroupAdded,
              (const tab_groups::TabGroupId& group_id),
              (override));
  MOCK_METHOD(void,
              OnTabGroupWillBeRemoved,
              (const tab_groups::TabGroupId& group_id),
              (override));

  void AddTabs(int num_tabs) {
    for (int i = 0; i < num_tabs; i++) {
      chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, false);
    }
  }

  void PrepareTabs(int num_tabs) {
    for (int i = 0; i < num_tabs; i++) {
      if (!browser()->tab_strip_model()->ContainsIndex(i)) {
        chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), i, false);
      }
      SetID(browser()->tab_strip_model()->GetWebContentsAt(i), i);
    }
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  }

  void PrepareTabstripForSelectionTest(TabStripModel* model,
                                       int tab_count,
                                       int pinned_count,
                                       const std::vector<int> selected_tabs) {
    ::PrepareTabstripForSelectionTest(
        base::BindOnce(&TabStripModelBrowserTest::PrepareTabs,
                       base::Unretained(this)),
        model, tab_count, pinned_count, selected_tabs);
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedObservation<TabStripModel, TabStripModelBrowserTest> observer_{
      this};
};

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, OnTabGroupAdded) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // We should already have a tab. Add it to a group and see if
  // TabStripModelObserver::OnTabGroupAdded is called.
  EXPECT_CALL(*this, OnTabGroupAdded(_)).Times(1);

  observer_.Observe(browser()->tab_strip_model());
  browser()->tab_strip_model()->AddToNewGroup({0});
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, OnTabGroupWillBeRemoved) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  // Close the group and see if TabStripModelObserver::OnTabGroupWillBeRemoved
  // is called.
  EXPECT_CALL(*this, OnTabGroupWillBeRemoved(group_id)).Times(1);

  observer_.Observe(browser()->tab_strip_model());
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest,
                       DetachWebContentsAtForInsertion) {
  class WebContentsRemovedObserver : public TabStripModelObserver {
   public:
    WebContentsRemovedObserver() = default;
    WebContentsRemovedObserver(const WebContentsRemovedObserver&) = delete;
    WebContentsRemovedObserver& operator=(const WebContentsRemovedObserver&) =
        delete;
    ~WebContentsRemovedObserver() override = default;

    // TabStripModelObserver:
    void OnTabStripModelChanged(
        TabStripModel* tab_strip_model,
        const TabStripModelChange& change,
        const TabStripSelectionChange& selection) override {
      if (change.type() == TabStripModelChange::kRemoved) {
        const TabStripModelChange::RemovedTab& removed_tab =
            change.GetRemove()->contents[0];
        remove_reason_ = removed_tab.remove_reason;
        tab_detach_reason_ = removed_tab.tab_detach_reason;
      }
    }

    std::optional<TabRemovedReason> remove_reason() const {
      return remove_reason_;
    }
    std::optional<tabs::TabInterface::DetachReason> tab_detach_reason() const {
      return tab_detach_reason_;
    }

   private:
    std::optional<TabRemovedReason> remove_reason_;
    std::optional<tabs::TabInterface::DetachReason> tab_detach_reason_;
  };

  // Start with a browser window with 2 tabs.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* const initial_tab = tab_strip_model->GetTabAtIndex(1);
  EXPECT_EQ(2, tab_strip_model->count());

  base::MockCallback<tabs::TabInterface::WillDetach> tab_detached_callback;

  base::CallbackListSubscription tab_subscription =
      initial_tab->RegisterWillDetach(tab_detached_callback.Get());
  WebContentsRemovedObserver removed_observer;
  tab_strip_model->AddObserver(&removed_observer);

  // Extract the new WebContents for re-insertion.
  EXPECT_CALL(tab_detached_callback,
              Run(tab_strip_model->GetTabAtIndex(1),
                  tabs::TabInterface::DetachReason::kDelete));
  std::unique_ptr<content::WebContents> extracted_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  EXPECT_EQ(TabRemovedReason::kInsertedIntoOtherTabStrip,
            removed_observer.remove_reason());
  EXPECT_EQ(tabs::TabInterface::DetachReason::kDelete,
            removed_observer.tab_detach_reason());
  tab_strip_model->AppendWebContents(std::move(extracted_contents), true);
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest,
                       ReplaceActiveTabWhenDeletesGroupShowsDeletionDialog) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  AddTabs(4);
  EXPECT_EQ(tab_strip_model->count(), 5);

  // Enter a zero state split. This adds a new tab.
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);

  // Add tab at index 4 to a group.
  tab_groups::TabGroupId group_id = tab_strip_model->AddToNewGroup({4});
  tab_strip_model->ActivateTabAt(1);

  tab_strip_model->UpdateTabInSplit(tab_strip_model->GetTabAtIndex(1), 4,
                                    TabStripModel::SplitUpdateType::kReplace);

  // Make sure the dialog is shown, and fake clicking the button.
  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser()->GetFeatures().tab_group_deletion_dialog_controller();
  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  // Pull the dialog state and call the OnDialogOk method.
  deletion_dialog_controller->SimulateOkButtonForTesting();

  EXPECT_FALSE(tab_strip_model->group_model()->ContainsTabGroup(group_id));
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(0)->GetSplit().has_value());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(1)->GetSplit().has_value());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandTogglePinned.
IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, CommandAddToSplit) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  AddTabs(4);
  EXPECT_EQ(tab_strip_model->count(), 5);
  tab_strip_model->SetTabPinned(0, true);
  tab_strip_model->SetTabPinned(1, true);

  // Add tab at index 4 to a group.
  tab_strip_model->AddToNewGroup({4});

  tab_strip_model->ActivateTabAt(3);
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAddToSplit));
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      1, TabStripModel::CommandAddToSplit));
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      2, TabStripModel::CommandAddToSplit));
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      3, TabStripModel::CommandAddToSplit));
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      4, TabStripModel::CommandAddToSplit));

  tab_strip_model->ExecuteContextMenuCommand(0,
                                             TabStripModel::CommandAddToSplit);

  // The first tab should become unpinned and adjacent to the active tab.
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(0)->IsPinned());
  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(1)->IsPinned());
  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(1)->IsSplit());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(2)->IsSplit());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(3)->IsSplit());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(4)->GetGroup().has_value());
  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(4)->IsSplit());
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest,
                       CommandAddToSplitWhenDeletesGroupShowsDeletionDialog) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  AddTabs(4);
  EXPECT_EQ(tab_strip_model->count(), 5);

  // Add tab at index 4 to a group.
  tab_groups::TabGroupId group_id = tab_strip_model->AddToNewGroup({4});

  tab_strip_model->ActivateTabAt(3);
  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      4, TabStripModel::CommandAddToSplit));

  tab_strip_model->ExecuteContextMenuCommand(4,
                                             TabStripModel::CommandAddToSplit);

  // Make sure the dialog is shown, and fake clicking the button.
  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser()->GetFeatures().tab_group_deletion_dialog_controller();
  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  // Pull the dialog state and call the OnDialogOk method.
  deletion_dialog_controller->SimulateOkButtonForTesting();

  EXPECT_FALSE(tab_strip_model->group_model()->ContainsTabGroup(group_id));
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(3)->GetSplit().has_value());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(4)->GetSplit().has_value());
}

// Calling duplicate on a tab that isn't selected doesn't affect selected tabs.
IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, CommandDuplicate) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();

  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(tab_strip_model, 3, 1, {0, 1}));
  ASSERT_EQ("0p 1 2", GetTabStripStateString(tab_strip_model));

  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      2, TabStripModel::CommandDuplicate));
  tab_strip_model->ExecuteContextMenuCommand(2,
                                             TabStripModel::CommandDuplicate);
  // Should have duplicated tab 2.
  EXPECT_EQ("0p 1 2 -1", GetTabStripStateString(tab_strip_model));
}

// Calling duplicate on a split that isn't selected doesn't affect selected
// tabs.
IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, CommandDuplicateSplit) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();

  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(tab_strip_model, 4, 1, {2}));
  tab_strip_model->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_EQ("0p 1 2s 3s", GetTabStripStateString(tab_strip_model));
  tab_strip_model->ActivateTabAt(1);

  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      2, TabStripModel::CommandDuplicate));
  tab_strip_model->ExecuteContextMenuCommand(2,
                                             TabStripModel::CommandDuplicate);
  // Should have duplicated split with tabs 2 and 3.
  EXPECT_EQ("0p 1 2s 3s -1s -1s", GetTabStripStateString(tab_strip_model));
}

// Calling duplicate on a tab that is selected affects all the selected tabs.
IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, CommandDuplicateSelected) {
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();

  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(tab_strip_model, 12, 6, {2}));
  tab_strip_model->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_strip_model->ActivateTabAt(4);
  tab_strip_model->AddToNewSplit(
      {5}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_strip_model->ActivateTabAt(8);
  tab_strip_model->AddToNewSplit(
      {9}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_strip_model->ActivateTabAt(10);
  tab_strip_model->AddToNewSplit(
      {11}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_EQ("0p 1p 2ps 3ps 4ps 5ps 6 7 8s 9s 10s 11s",
            GetTabStripStateString(tab_strip_model));
  tab_strip_model->ActivateTabAt(1);
  tab_strip_model->SelectTabAt(4);
  tab_strip_model->SelectTabAt(5);
  tab_strip_model->SelectTabAt(7);
  tab_strip_model->SelectTabAt(10);
  tab_strip_model->SelectTabAt(11);

  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      1, TabStripModel::CommandDuplicate));
  tab_strip_model->ExecuteContextMenuCommand(1,
                                             TabStripModel::CommandDuplicate);
  // Should have duplicated tabs 1, 4, 5, 7, 10, and 11.
  EXPECT_EQ("0p 1p -1p 2ps 3ps 4ps 5ps -1ps -1ps 6 7 -1 8s 9s 10s 11s -1s -1s",
            GetTabStripStateString(tab_strip_model));
}

// TODO(crbug.com/501991031): Fails on "chrome/ci/linux-chromeos-chrome".
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestCloseTabDuringMoveOperation \
  DISABLED_TestCloseTabDuringMoveOperation
#else
#define MAYBE_TestCloseTabDuringMoveOperation TestCloseTabDuringMoveOperation
#endif
IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest,
                       MAYBE_TestCloseTabDuringMoveOperation) {
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/501991031): Fails on "try/network_service_linux".
  if (base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest)) {
    GTEST_SKIP();
  }
#endif

  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(tab_strip_model, 2, 0, {0}));
  ASSERT_EQ(2, tab_strip_model->count());

  class TabStripModelCloseWebContentsOnChangeObserver
      : public TabStripModelObserver {
   public:
    void OnTabStripModelChanged(
        TabStripModel* tab_strip_model,
        const TabStripModelChange& change,
        const TabStripSelectionChange& selection) override {
      if (change.type() == TabStripModelChange::Type::kMoved) {
        tab_strip_model->CloseWebContentsAt(1,
                                            TabCloseTypes::CLOSE_USER_GESTURE);
      }
    }
  };
  TabStripModelCloseWebContentsOnChangeObserver close_tab_observer;
  tab_strip_model->AddObserver(&close_tab_observer);

  EXPECT_DEATH_IF_SUPPORTED(tab_strip_model->MoveWebContentsAt(0, 1, false),
                            "Check failed");
  tab_strip_model->RemoveObserver(&close_tab_observer);
}

class TabStripModelTestTabGroupEntryPointsEnabled
    : public TabStripModelBrowserTest {
 public:
  TabStripModelTestTabGroupEntryPointsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTabGroupMenuMoreEntryPoints);
  }

  TabStrip* tabstrip() {
    return views::AsViewClass<HorizontalTabStripRegionView>(
               browser()->GetBrowserView().tab_strip_view())
        ->tab_strip();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripModelTestTabGroupEntryPointsEnabled,
                       TestMostRecentlyUsedGroup) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());
  ASSERT_TRUE(tab_group_model);

  TabStripController* tab_strip_controller = tabstrip()->controller();
  tab_strip_controller->CreateNewTab(NewTabTypes::kNewTabCommand);
  tab_strip_controller->CreateNewTab(NewTabTypes::kNewTabCommand);
  tab_strip_controller->CreateNewTab(NewTabTypes::kNewTabCommand);

  ASSERT_TRUE(tab_strip_model->count() == 4);

  const tab_groups::TabGroupId group_1 = tab_strip_model->AddToNewGroup({1});
  const tab_groups::TabGroupId group_2 = tab_strip_model->AddToNewGroup({2});
  const tab_groups::TabGroupId group_3 = tab_strip_model->AddToNewGroup({3});

  tab_strip_model->ActivateTabAt(2);
  tab_strip_model->ActivateTabAt(1);
  tab_strip_model->ActivateTabAt(3);
  tab_strip_model->ActivateTabAt(0);

  std::optional<tab_groups::TabGroupId> most_recently_used =
      tab_group_model->GetMostRecentTabGroupId();

  EXPECT_TRUE(most_recently_used);
  EXPECT_EQ(*most_recently_used, group_3);

  tab_strip_controller->RemoveTabFromGroup(3);
  most_recently_used = tab_group_model->GetMostRecentTabGroupId();
  EXPECT_TRUE(most_recently_used);
  EXPECT_EQ(*most_recently_used, group_1);

  tab_strip_controller->RemoveTabFromGroup(1);
  most_recently_used = tab_group_model->GetMostRecentTabGroupId();
  EXPECT_TRUE(most_recently_used);
  EXPECT_EQ(*most_recently_used, group_2);

  tab_strip_controller->RemoveTabFromGroup(2);
  EXPECT_FALSE(tab_group_model->GetMostRecentTabGroupId());
}
