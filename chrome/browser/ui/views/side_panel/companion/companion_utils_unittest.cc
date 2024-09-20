// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace companion {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
class CompanionUtilsTest : public BrowserWithTestWindowTest {
 public:
  CompanionUtilsTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::internal::kSidePanelCompanionChromeOS}, {});
  }
  ~CompanionUtilsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void RegisterPrefs(PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar,
                           false);
  pref_service->SetBoolean(companion::kExpsOptInStatusGrantedPref, false);
}

TEST_F(CompanionUtilsTest, DISABLED_PinnedStateCommandlineOverridePinned) {
  PrefService* const pref_service = browser()->profile()->GetPrefs();
  RegisterPrefs(pref_service);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      companion::switches::kForceCompanionPinnedState, "pinned");

  companion::UpdateCompanionDefaultPinnedToToolbarState(browser()->profile());
  EXPECT_EQ(
      pref_service->GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      true);

  // Expect the companion state was not added to the pinned actions list.
  EXPECT_EQ(0u, pref_service->GetList(prefs::kPinnedActions).size());
}

TEST_F(CompanionUtilsTest, DISABLED_PinnedStateCommandlineOverrideUnpinned) {
  PrefService* const pref_service = browser()->profile()->GetPrefs();

  RegisterPrefs(pref_service);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceCompanionPinnedState, "unpinned");

  companion::UpdateCompanionDefaultPinnedToToolbarState(profile());
  EXPECT_EQ(
      pref_service->GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      false);

  // Expect the companion state was not added to the pinned actions list.
  EXPECT_EQ(0u, pref_service->GetList(prefs::kPinnedActions).size());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)

}  // namespace companion
