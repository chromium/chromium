// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_ui.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class OrganizerPanelUIBrowserTest : public InProcessBrowserTest {
 public:
  OrganizerPanelUIBrowserTest() {
    feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelUIBrowserTest, GetProfileLoadTimeData) {
  Profile* profile = browser()->GetProfile();
  base::DictValue dict = OrganizerPanelUI::GetProfileLoadTimeData(profile);
  EXPECT_EQ(false, dict.FindBool("isUserSignedIn"));
  EXPECT_EQ(false, dict.FindBool("isIncognitoMode"));

  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), "user@example.com",
      signin::ConsentLevel::kSignin);
  dict = OrganizerPanelUI::GetProfileLoadTimeData(profile);
  EXPECT_EQ(true, dict.FindBool("isUserSignedIn"));
  EXPECT_EQ(false, dict.FindBool("isIncognitoMode"));

  dict = OrganizerPanelUI::GetProfileLoadTimeData(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_EQ(true, dict.FindBool("isIncognitoMode"));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUIBrowserTest, RecordsLoadMetrics) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOrganizerPanelURL)));

  histogram_tester.ExpectTotalCount("Tabs.Organizer.WebUI.LoadDocumentTime", 1);
  histogram_tester.ExpectTotalCount("Tabs.Organizer.WebUI.LoadCompletedTime",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.PageHandlerConstructionDelay", 1);
}

}  // namespace
