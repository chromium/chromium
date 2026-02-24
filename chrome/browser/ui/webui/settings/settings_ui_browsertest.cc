// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_ui.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

typedef InProcessBrowserTest SettingsUITest;

using ::testing::_;
using ui_test_utils::NavigateToURL;

IN_PROC_BROWSER_TEST_F(SettingsUITest, ViewSourceDoesntCrash) {
  ASSERT_TRUE(NavigateToURL(
      browser(),
      GURL(content::kViewSourceScheme + std::string(":") +
           chrome::kChromeUISettingsURL + std::string("strings.js"))));
}

// Catch lifetime issues in message handlers. There was previously a problem
// with PrefMember calling Init again after Destroy.
IN_PROC_BROWSER_TEST_F(SettingsUITest, ToggleJavaScript) {
  ASSERT_TRUE(NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL)));

  const auto& handlers = *browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetWebUI()
                              ->GetHandlersForTesting();

  for (const std::unique_ptr<content::WebUIMessageHandler>& handler :
       handlers) {
    handler->AllowJavascriptForTesting();
    handler->DisallowJavascript();
    handler->AllowJavascriptForTesting();
  }
}

IN_PROC_BROWSER_TEST_F(SettingsUITest, TriggerHappinessTrackingSurveys) {
  MockHatsService* mock_hats_service_ = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerSettings, _,
                                                _, _, _, _, _, _, _, _));
  ASSERT_TRUE(NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL)));
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test fixture for testing the kDisableSync flag.
class SettingsUITestDisableSync : public SettingsUITest {
 public:
  void SetUp() override {
    // Append the switch *before* the profile is built.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
    SettingsUITest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kUnoPhase2FollowUp};
};

// Regression test for crbug.com/484893496.
// This mainly intends to check that `CreateBatchUploadPromoHandler()` does not
// crash when the sync service is null.
IN_PROC_BROWSER_TEST_F(
    SettingsUITestDisableSync,
    CreateBatchUploadPromoHandlerWithoutSyncServiceDoesNotCrash) {
  ASSERT_TRUE(
      NavigateToURL(browser(), GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                                  chrome::kPeopleSubPage}))));

  ASSERT_EQ(nullptr, SyncServiceFactory::GetForProfile(browser()->profile()));

  // Wait for sync controls to load which would initialize the batch upload
  // service if the sync service was not null.
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"((() => {
                           return customElements.whenDefined(
                              'settings-sync-controls');
                         })())"));

  EXPECT_EQ(nullptr,
            BatchUploadServiceFactory::GetForProfile(browser()->profile(),
                                                     /*create=*/false));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
