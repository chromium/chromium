// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/events/event_constants.h"

namespace glic {
namespace {

class GlicButtonTest : public InProcessBrowserTest {
 public:
  GlicButtonTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    glic::ForceSigninAndModelExecutionCapability(browser()->profile());
  }

 protected:
  GlicButton* glic_button() {
    return browser()
        ->GetBrowserView()
        .tab_strip_region_view()
        ->GetTabStripActionContainer()
        ->GetGlicButton();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuPinned) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_TRUE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuUnpinned) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, false);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_FALSE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, UnpinCommand) {
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ExecuteCommand(IDC_GLIC_TOGGLE_PIN, ui::EF_NONE);
  EXPECT_FALSE(profile_prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
}
}  // namespace
}  // namespace glic
