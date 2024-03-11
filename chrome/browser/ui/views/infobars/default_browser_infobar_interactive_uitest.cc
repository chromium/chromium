// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/infobar_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
constexpr char kInfoBarAcceptButton[] = "infobar_accept_button";
}  // namespace

class DefaultBrowserInfobarInteractiveTest : public InteractiveBrowserTest {
 public:
  ConfirmInfoBar* GetActiveInfoBar() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    CHECK(infobar_manager);
    return static_cast<ConfirmInfoBar*>(infobar_manager->infobars()[0]);
  }

  auto NameAcceptButton() {
    return NameView(kInfoBarAcceptButton, base::BindLambdaForTesting([&]() {
                      return static_cast<views::View*>(
                          GetActiveInfoBar()->ok_button_for_testing());
                    }));
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarInteractiveTest,
                       ShowsDefaultBrowserPrompt) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

class DefaultBrowserInfobarWithRefreshInteractiveTest
    : public DefaultBrowserInfobarInteractiveTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDefaultBrowserPromptRefresh);

    DefaultBrowserInfobarInteractiveTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       ShowsDefaultBrowserPromptOnNewTab) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       RemovesAllBrowserPromptsOnAccept) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      NameAcceptButton(), PressButton(kInfoBarAcceptButton), FlushEvents(),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      SelectTab(kTabStripElementId, 0), FlushEvents(),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       LogsMetrics) {
  base::HistogramTester histogram_tester;
  ShowPromptForTesting();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
                  NameAcceptButton(), PressButton(kInfoBarAcceptButton),
                  FlushEvents(), WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                  FlushEvents());

  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.InfoBar.TimesShownBeforeAccept", 1);
}
