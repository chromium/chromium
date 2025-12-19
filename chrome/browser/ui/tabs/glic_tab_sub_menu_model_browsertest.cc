// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
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

class GlicTabSubMenuModelTest : public InProcessBrowserTest {
 public:
  GlicTabSubMenuModelTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
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
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(glic::prefs::FreStatus::kCompleted));
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

}  // namespace glic
