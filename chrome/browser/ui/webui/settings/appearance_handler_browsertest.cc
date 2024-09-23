// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace settings {

class AppearanceHandlerTest : public InProcessBrowserTest {
 public:
  AppearanceHandlerTest() = default;
  ~AppearanceHandlerTest() override = default;
  AppearanceHandlerTest(const AppearanceHandlerTest&) = delete;
  AppearanceHandlerTest& operator=(const AppearanceHandlerTest&) = delete;

  void SetUpOnMainThread() override {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::GetSettingsUrl(chrome::kAppearanceSubPage))));
    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kToolbarPinning};
};

IN_PROC_BROWSER_TEST_F(AppearanceHandlerTest,
                       OpenCustomizeChromeToolbarSection) {
  base::Value::List args;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetWebUI()
      ->ProcessWebUIMessage(GURL(), "openCustomizeChromeToolbarSection",
                            std::move(args));
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  const std::optional<SidePanelEntryId> current_entry =
      browser()->GetFeatures().side_panel_ui()->GetCurrentEntryId();
  EXPECT_TRUE(current_entry.has_value());
  EXPECT_EQ(SidePanelEntryId::kCustomizeChrome, current_entry.value());
}

IN_PROC_BROWSER_TEST_F(AppearanceHandlerTest, ResetPinnedToolbarActions) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShowHomeButton, true);
  prefs->SetBoolean(prefs::kShowForwardButton, false);

  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowForwardButton));
  EXPECT_EQ(2u, actions_model->PinnedActionIds().size());

  base::Value::List args;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetWebUI()
      ->ProcessWebUIMessage(GURL(), "resetPinnedToolbarActions",
                            std::move(args));

  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowForwardButton));
  ASSERT_EQ(1u, actions_model->PinnedActionIds().size());
  EXPECT_EQ(kActionShowChromeLabs, actions_model->PinnedActionIds()[0]);
}

}  // namespace settings
