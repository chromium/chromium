// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

  GlicKeyedService* service = GlicKeyedService::Get(browser()->profile());
  ASSERT_TRUE(service);
  GlicWindowController& glic_instance_coordinator =
      service->window_controller();
  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);

  std::vector<tabs::TabHandle> handles_to_wait_for = {tab->GetHandle()};
  glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                    handles_to_wait_for);

  // Execute the Create new chat command via the glic submenu model.
  auto submenu_model =
      std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 0);
  submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

  // Wait for the tab to be pinned.
  waiter.Wait();

  GlicInstance* instance = glic_instance_coordinator.GetInstanceForTab(tab);

  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(glic_instance_coordinator.GetActiveInstance(), instance);

  // Verify the tab is pinned with the correct trigger.
  auto pinned_tab_usage =
      service->sharing_manager().GetPinnedTabUsage(tab->GetHandle());
  ASSERT_TRUE(pinned_tab_usage.has_value());
  EXPECT_EQ(pinned_tab_usage->pin_event.trigger,
            glic::GlicPinTrigger::kContextMenu);
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

  GlicKeyedService* service = GlicKeyedService::Get(browser()->profile());
  ASSERT_TRUE(service);
  GlicWindowController& glic_instance_coordinator =
      service->window_controller();
  tabs::TabInterface* tab0 = tab_strip_model->GetTabAtIndex(0);
  tabs::TabInterface* tab1 = tab_strip_model->GetTabAtIndex(1);

  std::vector<tabs::TabHandle> handles_to_wait_for = {tab0->GetHandle(),
                                                      tab1->GetHandle()};
  glic::GlicTabPinningWaiter waiter(&service->sharing_manager(),
                                    handles_to_wait_for);

  // Execute the Create new chat command via the glic submenu model.
  auto submenu_model =
      std::make_unique<GlicTabSubMenuModel>(tab_strip_model, 1);
  submenu_model->ExecuteCommand(TabStripModel::CommandGlicCreateNewChat, 0);

  // Wait for the tabs to be pinned.
  waiter.Wait();

  GlicInstance* instance1 = glic_instance_coordinator.GetInstanceForTab(tab0);
  GlicInstance* instance2 = glic_instance_coordinator.GetInstanceForTab(tab1);

  ASSERT_TRUE(instance1);
  ASSERT_TRUE(instance2);
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(instance1->IsShowing());
  EXPECT_TRUE(instance2->IsShowing());
  EXPECT_EQ(glic_instance_coordinator.GetActiveInstance(), instance1);

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

  // Tab 2 should not be bound or pinned to anything.
  GlicInstance* instance3 = glic_instance_coordinator.GetInstanceForTab(
      tab_strip_model->GetTabAtIndex(2));
  EXPECT_FALSE(instance3);
}

}  // namespace glic
