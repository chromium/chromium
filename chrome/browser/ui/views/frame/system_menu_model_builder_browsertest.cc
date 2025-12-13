// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
namespace {
// Returns true if there exists a visible command with specified id and
// (optionally) label in the given menu. False otherwise.
bool ContainsCommand(const ui::MenuModel* menu,
                     int command_id,
                     std::optional<int> label_id) {
  CHECK(menu);
  for (size_t index = 0; index < menu->GetItemCount(); index++) {
    if (menu->GetCommandIdAt(index) == command_id && menu->IsVisibleAt(index) &&
        (!label_id.has_value() ||
         menu->GetLabelAt(index) ==
             l10n_util::GetStringUTF16(label_id.value()))) {
      return true;
    }
  }
  return false;
}
}  // namespace

class SystemMenuModelBuilderGlicTest : public InProcessBrowserTest {
 private:
  glic::GlicTestEnvironment glic_test_env_;
};

// Check if the toggle glic pinning option exists and has the right label based
// on relevant prefs.
IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderGlicTest, TogglePinning) {
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  ui::MenuModel* menu = BrowserView::GetBrowserViewForBrowser(browser())
                            ->browser_widget()
                            ->GetSystemMenuModel();

  profile_prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  EXPECT_FALSE(ContainsCommand(menu, IDC_GLIC_TOGGLE_PIN, std::nullopt));

  profile_prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_TRUE(ContainsCommand(menu, IDC_GLIC_TOGGLE_PIN, IDS_GLIC_PIN));

  profile_prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(ContainsCommand(menu, IDC_GLIC_TOGGLE_PIN, IDS_GLIC_UNPIN));
}
#endif
