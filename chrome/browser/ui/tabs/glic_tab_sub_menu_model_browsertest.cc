// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace glic {

class GlicTabPinningWaiter {
 public:
  GlicTabPinningWaiter(glic::GlicSharingManager* sharing_manager,
                       const std::vector<tabs::TabHandle>& tab_handles)
      : sharing_manager_(sharing_manager) {
    for (const auto& handle : tab_handles) {
      if (!sharing_manager_->IsTabPinned(handle)) {
        tabs_to_wait_for_.insert(handle);
      }
    }
    if (!tabs_to_wait_for_.empty()) {
      subscription_ = sharing_manager_->AddTabPinningStatusChangedCallback(
          base::BindRepeating(&GlicTabPinningWaiter::OnTabPinningStatusChanged,
                              base::Unretained(this)));
    }
  }

  // Waits for all tabs to become pinned.
  void Wait() {
    if (tabs_to_wait_for_.empty()) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnTabPinningStatusChanged(tabs::TabInterface* tab, bool is_pinned) {
    if (!is_pinned) {
      return;
    }

    auto it = tabs_to_wait_for_.find(tab->GetHandle());
    if (it != tabs_to_wait_for_.end()) {
      tabs_to_wait_for_.erase(it);
      if (tabs_to_wait_for_.empty() && quit_closure_) {
        std::move(quit_closure_).Run();
      }
    }
  }

  raw_ptr<glic::GlicSharingManager> sharing_manager_;
  base::flat_set<tabs::TabHandle> tabs_to_wait_for_;
  base::CallbackListSubscription subscription_;
  base::OnceClosure quit_closure_;
};

class GlicTabSubMenuModelTest : public InProcessBrowserTest {
 public:
  GlicTabSubMenuModelTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicMultiInstance,
                              features::kGlicMITabContextMenu,
#if BUILDFLAG(IS_CHROMEOS)
                              chromeos::features::kFeatureManagementGlic
#endif
        },
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Skips FRE.
    command_line->AppendSwitch(switches::kGlicDev);
    command_line->AppendSwitch(switches::kGlicAutomation);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(glic::prefs::FreStatus::kCompleted));
  }

  void TearDownOnMainThread() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  GlicKeyedService* GetGlicKeyedService() {
    GlicKeyedService* service = GlicKeyedService::Get(browser()->profile());
    EXPECT_TRUE(service);
    return service;
  }

  GlicInstanceCoordinatorImpl* GetGlicInstanceCoordinator() {
    GlicKeyedService* service = GetGlicKeyedService();
    if (!service) {
      return nullptr;
    }
    return static_cast<GlicInstanceCoordinatorImpl*>(
        &service->window_controller());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, GlicSubMenuOpens) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  // Open the Tab Menu Model for the first tab. Ensure that
  // TabStripModel::CommandGlicShare is present in the menu.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  auto menu = std::make_unique<TabMenuModel>(
      /*delegate=*/nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/0);

  size_t index = 0;
  bool menu_command_found = false;
  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    if (menu->GetCommandIdAt(i) == TabStripModel::CommandGlicShare) {
      index = i;
      menu_command_found = true;
      break;
    }
  }
  ASSERT_TRUE(menu_command_found);

  EXPECT_EQ(ui::MenuModel::TYPE_SUBMENU, menu->GetTypeAt(index));
  ui::MenuModel* submenu = menu->GetSubmenuModelAt(index);
  ASSERT_TRUE(submenu);

  // Check that the submenu contains the option to start a new chat.
  bool submenu_command_found = false;
  for (size_t i = 0; i < submenu->GetItemCount(); ++i) {
    if (submenu->GetCommandIdAt(i) == TabStripModel::CommandGlicCreateNewChat) {
      submenu_command_found = true;
      break;
    }
  }
  EXPECT_TRUE(submenu_command_found);
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, CreateNewChatWithSingleTab) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_GE(tab_strip_model->count(), 1);

  // Select tab 0
  tab_strip_model->ActivateTabAt(0);

  GlicKeyedService* service = GetGlicKeyedService();
  ASSERT_TRUE(service);
  GlicInstanceCoordinatorImpl* glic_instance_coordinator =
      GetGlicInstanceCoordinator();
  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);

  std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
  glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                    handles_to_wait_for);

  // Execute the Create new chat command via the glic submenu model.
  base::HistogramTester histogram_tester;
  auto submenu_model =
      std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 0);
  submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

  // Wait for the tab to be pinned.
  waiter.Wait();

  GlicInstance* instance = glic_instance_coordinator->GetInstanceForTab(tab);

  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(glic_instance_coordinator->GetActiveInstance(), instance);

  // Verify the tab is pinned with the correct trigger.
  auto pinned_tab_usage =
      service->sharing_manager().GetPinnedTabUsage(tab->GetHandle());
  ASSERT_TRUE(pinned_tab_usage.has_value());
  EXPECT_EQ(pinned_tab_usage->pin_event.trigger,
            glic::GlicPinTrigger::kContextMenu);

  histogram_tester.ExpectBucketCount(
      "Glic.TabContextMenu.PinnedTabsToNewConversation", 1, 1);
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, CreateNewChatWithMultipleTabs) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(2, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_strip_model->count());

  // Select tabs 0 and 1
  ui::ListSelectionModel selection;
  selection.AddIndexToSelection(0);
  selection.AddIndexToSelection(1);
  selection.set_active(1);
  tab_strip_model->SetSelectionFromModel(selection);

  GlicKeyedService* service = GetGlicKeyedService();
  ASSERT_TRUE(service);
  GlicInstanceCoordinatorImpl* glic_instance_coordinator =
      GetGlicInstanceCoordinator();
  tabs::TabInterface* tab0 = tab_strip_model->GetTabAtIndex(0);
  tabs::TabInterface* tab1 = tab_strip_model->GetTabAtIndex(1);

  std::vector<tabs::TabHandle> handles_to_wait_for = {tab0->GetHandle(),
                                                      tab1->GetHandle()};
  glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                    handles_to_wait_for);

  // Execute the Create new chat command via the glic submenu model.
  base::HistogramTester histogram_tester;
  auto submenu_model =
      std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 1);
  submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

  // Wait for the tabs to be pinned.
  waiter.Wait();

  GlicInstance* instance1 = glic_instance_coordinator->GetInstanceForTab(tab0);
  GlicInstance* instance2 = glic_instance_coordinator->GetInstanceForTab(tab1);

  ASSERT_TRUE(instance1);
  ASSERT_TRUE(instance2);
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(instance1->IsShowing());
  EXPECT_TRUE(instance2->IsShowing());
  EXPECT_EQ(glic_instance_coordinator->GetActiveInstance(), instance1);

  // Verify tabs 0 and 1 are pinned with the correct trigger.
  auto pinned_tab_usage0 =
      service->sharing_manager().GetPinnedTabUsage(tab0->GetHandle());
  ASSERT_TRUE(pinned_tab_usage0.has_value());
  EXPECT_EQ(pinned_tab_usage0->pin_event.trigger,
            glic::GlicPinTrigger::kContextMenu);

  auto pinned_tab_usage1 =
      service->sharing_manager().GetPinnedTabUsage(tab1->GetHandle());
  ASSERT_TRUE(pinned_tab_usage1.has_value());
  EXPECT_EQ(pinned_tab_usage1->pin_event.trigger,
            glic::GlicPinTrigger::kContextMenu);

  histogram_tester.ExpectBucketCount(
      "Glic.TabContextMenu.PinnedTabsToNewConversation", 2, 1);

  // Tab 2 should not be bound or pinned to anything.
  GlicInstance* instance3 = glic_instance_coordinator->GetInstanceForTab(
      tab_strip_model->GetTabAtIndex(2));
  EXPECT_FALSE(instance3);
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, SwitchToRecentConversation) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  auto* glic_instance_coordinator = GetGlicInstanceCoordinator();
  ASSERT_TRUE(glic_instance_coordinator);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create 5 conversations by adding 5 tabs and activating the side panel for
  // each tab.
  for (int i = 1; i <= 5; ++i) {
    ASSERT_TRUE(
        AddTabAtIndex(i, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
    tab_strip_model->ActivateTabAt(i);
    tabs::TabInterface* current_tab = tab_strip_model->GetTabAtIndex(i);

    glic_instance_coordinator->Toggle(
        browser(),
        /*prevent_close=*/false,
        glic::mojom::InvocationSource::kTopChromeButton,
        /*prompt_suggestion=*/std::nullopt,
        /*auto_send=*/false,
        /*conversation_id=*/std::nullopt);

    // Wait for the instance to be shown and associated with the current tab.
    GlicInstance* instance = nullptr;
    ASSERT_TRUE(base::test::RunUntil([&]() {
      instance = glic_instance_coordinator->GetInstanceForTab(current_tab);
      return instance &&
             glic_instance_coordinator->GetActiveInstance() == instance &&
             instance->IsShowing();
    }));

    GlicInstanceImpl* instance_impl = static_cast<GlicInstanceImpl*>(instance);

    auto info = glic::mojom::ConversationInfo::New();
    info->conversation_id = "conv" + base::NumberToString(i);
    info->conversation_title = "Title " + base::NumberToString(i);
    instance_impl->RegisterConversation(std::move(info), base::DoNothing());

    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  auto recents = glic_instance_coordinator->GetRecentlyActiveConversations(10);
  ASSERT_EQ(5u, recents.size());

  auto menu = std::make_unique<TabMenuModel>(
      /*delegate=*/nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/0);

  std::optional<size_t> share_index =
      menu->GetIndexOfCommandId(TabStripModel::CommandGlicShare);
  ASSERT_TRUE(share_index.has_value());

  ui::MenuModel* submenu = menu->GetSubmenuModelAt(share_index.value());
  ASSERT_TRUE(submenu);

  // Verify that the menu contains "Create new chat" and a separator
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            TabStripModel::CommandGlicCreateNewChat);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);

  // Verify all 5 conversations are present and in the correct order (most
  // recent first).
  for (size_t i = 0; i < 5; ++i) {
    std::u16string expected_title =
        base::UTF8ToUTF16("Title " + base::NumberToString(5 - i));
    // Offset by 2 for "Create new chat" and the separator
    EXPECT_EQ(submenu->GetLabelAt(i + 2), expected_title);
  }

  // Now verify that we can switch a tab to a recent conversation.
  // Select tabs 1 and 2.
  ui::ListSelectionModel selection;
  selection.AddIndexToSelection(1);
  selection.AddIndexToSelection(2);
  selection.set_active(1);
  tab_strip_model->SetSelectionFromModel(selection);

  menu = std::make_unique<TabMenuModel>(
      /*delegate=*/nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/1);

  share_index = menu->GetIndexOfCommandId(TabStripModel::CommandGlicShare);
  ASSERT_TRUE(share_index.has_value());
  submenu = menu->GetSubmenuModelAt(share_index.value());
  ASSERT_TRUE(submenu);

  // Select the third conversation and pin tabs to it.
  size_t target_index = 0;
  bool found_target = false;
  for (size_t i = 0; i < submenu->GetItemCount(); ++i) {
    if (submenu->GetLabelAt(i) == u"Title 3") {
      target_index = i;
      found_target = true;
      break;
    }
  }
  ASSERT_TRUE(found_target);
  base::HistogramTester histogram_tester;
  submenu->ActivatedAt(target_index);

  tabs::TabInterface* tab1 = tab_strip_model->GetTabAtIndex(1);
  tabs::TabInterface* tab2 = tab_strip_model->GetTabAtIndex(2);

  // Verify tabs 1 and 2 are pinned to conv3.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* inst1 = glic_instance_coordinator->GetInstanceForTab(tab1);
    auto* inst2 = glic_instance_coordinator->GetInstanceForTab(tab2);
    // conv3
    return inst1 &&
           static_cast<GlicInstanceImpl*>(inst1)->conversation_id() ==
               "conv3" &&
           inst2 &&
           static_cast<GlicInstanceImpl*>(inst2)->conversation_id() == "conv3";
  }));

  histogram_tester.ExpectBucketCount(
      "Glic.TabContextMenu.PinnedTabsToExistingConversation", 2, 1);
}

class TestMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  explicit TestMenuDelegate(TabStripModel* tab_strip_model, int index)
      : tab_strip_model_(tab_strip_model), index_(index) {}

  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override {
    tab_strip_model_->ExecuteContextMenuCommand(
        index_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
  int index_;
};

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest,
                       UnshareCommandHiddenWhenNothingIsPinned) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_GE(tab_strip_model->count(), 1);

  // Select the first tab
  tab_strip_model->ActivateTabAt(0);

  // Open the context menu without pinning anything
  TestMenuDelegate delegate(tab_strip_model, 0);
  auto menu = std::make_unique<TabMenuModel>(
      &delegate, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/0);

  // Verify that the "Unshare with Gemini" command isn't shown
  bool unshare_command_found = false;
  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    if (menu->GetCommandIdAt(i) == TabStripModel::CommandGlicUnshare) {
      unshare_command_found = true;
      break;
    }
  }
  EXPECT_FALSE(unshare_command_found);
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, UnshareCommandShown) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  // Add a second tab so we have one pinned and one unpinned.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_GE(tab_strip_model->count(), 2);

  // Select the first tab and pin it to a conversation.
  tab_strip_model->ActivateTabAt(0);

  GlicKeyedService* service = GlicKeyedService::Get(browser()->profile());
  ASSERT_TRUE(service);

  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);

  std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
  glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                    handles_to_wait_for);

  service->window_controller().CreateNewConversationForTabs({tab});
  waiter.Wait();

  // Select both tabs and open the context menu.
  ui::ListSelectionModel selection;
  selection.AddIndexToSelection(0);
  selection.AddIndexToSelection(1);
  selection.set_active(0);
  tab_strip_model->SetSelectionFromModel(selection);

  TestMenuDelegate delegate(tab_strip_model, 0);
  auto menu = std::make_unique<TabMenuModel>(
      &delegate, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/0);

  // Verify that the "Unshare with Gemini" command is shown
  int unshare_command_index = -1;
  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    if (menu->GetCommandIdAt(i) == TabStripModel::CommandGlicUnshare) {
      unshare_command_index = i;
      break;
    }
  }
  EXPECT_NE(unshare_command_index, -1);
  EXPECT_TRUE(menu->IsEnabledAt(unshare_command_index));

  base::HistogramTester histogram_tester;
  menu->ActivatedAt(unshare_command_index);
  histogram_tester.ExpectBucketCount("Glic.TabContextMenu.UnpinnedTabs", 2, 1);
}

IN_PROC_BROWSER_TEST_F(
    GlicTabSubMenuModelTest,
    UnshareCommandShownForBackgroundTabInDifferentConversation) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  // Add a second tab so we have one pinned and one unpinned.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_GE(tab_strip_model->count(), 2);

  // Select the first tab and pin it to a new conversation.
  tab_strip_model->ActivateTabAt(0);

  GlicKeyedService* service = GlicKeyedService::Get(browser()->profile());
  ASSERT_TRUE(service);

  tabs::TabInterface* tab0 = tab_strip_model->GetTabAtIndex(0);
  tabs::TabInterface* tab1 = tab_strip_model->GetTabAtIndex(1);

  {
    std::vector<tabs::TabHandle> handles_to_wait_for = {tab0->GetHandle()};
    glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                      handles_to_wait_for);
    service->window_controller().CreateNewConversationForTabs({tab0});
    waiter.Wait();
  }

  // Create a new conversation for the second tab.
  tab_strip_model->ActivateTabAt(1);
  {
    std::vector<tabs::TabHandle> handles_to_wait_for = {tab1->GetHandle()};
    glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                      handles_to_wait_for);
    service->window_controller().CreateNewConversationForTabs({tab1});
    waiter.Wait();
  }

  // Open the context menu for the first tab.
  // This tests the background/inactive conversation pinned status.
  TestMenuDelegate delegate(tab_strip_model, 0);
  auto menu = std::make_unique<TabMenuModel>(
      &delegate, browser()->GetFeatures().tab_menu_model_delegate(),
      tab_strip_model, /*index=*/0);

  // Verify that the "Unshare with Gemini" command is shown
  int unshare_command_index = -1;
  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    if (menu->GetCommandIdAt(i) == TabStripModel::CommandGlicUnshare) {
      unshare_command_index = i;
      break;
    }
  }
  EXPECT_NE(unshare_command_index, -1);
  EXPECT_TRUE(menu->IsEnabledAt(unshare_command_index));
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest, UnpinThenRepinTab) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_GE(tab_strip_model->count(), 1);

  // Select the first tab and pin it to a new conversation.
  tab_strip_model->ActivateTabAt(0);

  GlicKeyedService* service = GetGlicKeyedService();
  ASSERT_TRUE(service);
  GlicInstanceCoordinatorImpl* glic_instance_coordinator =
      GetGlicInstanceCoordinator();
  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);

  {
    std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
    glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                      handles_to_wait_for);

    auto submenu_model =
        std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 0);
    submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

    waiter.Wait();
  }

  GlicInstance* instance = glic_instance_coordinator->GetInstanceForTab(tab);
  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_TRUE(service->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Unpin that tab.
  service->sharing_manager().UnpinTabs({tab->GetHandle()});
  EXPECT_FALSE(service->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Re-pin that tab.
  {
    std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
    glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                      handles_to_wait_for);

    auto submenu_model =
        std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 0);
    submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

    waiter.Wait();
  }

  EXPECT_TRUE(service->sharing_manager().IsTabPinned(tab->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicTabSubMenuModelTest,
                       UnpinThenNavigateToOtherTabAndRemainsUnpinned) {
  // Ensure Glic is enabled for the profile.
  EXPECT_TRUE(GlicEnabling::IsReadyForProfile(browser()->profile()));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  if (tab_strip_model->count() < 2) {
    ASSERT_TRUE(
        AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  }
  ASSERT_GE(tab_strip_model->count(), 2);

  // Select the first tab and pin it to a new conversation.
  tab_strip_model->ActivateTabAt(0);

  GlicKeyedService* service = GetGlicKeyedService();
  ASSERT_TRUE(service);

  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);

  {
    std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
    glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                      handles_to_wait_for);
    service->window_controller().CreateNewConversationForTabs({tab});
    waiter.Wait();
  }
  EXPECT_TRUE(service->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Unpin that tab.
  service->sharing_manager().UnpinTabs({tab->GetHandle()});
  EXPECT_FALSE(service->sharing_manager().IsTabPinned(tab->GetHandle()));

  // Switch to the other tab.
  tab_strip_model->ActivateTabAt(1);

  // Wait for the instance to go away.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !service->GetInstanceForTab(tab)->IsShowing(); }));

  // Switch to tab 0.
  tab_strip_model->ActivateTabAt(0);

  // Verify it is still unpinned.
  EXPECT_FALSE(service->sharing_manager().IsTabPinned(tab->GetHandle()));
}

}  // namespace glic
