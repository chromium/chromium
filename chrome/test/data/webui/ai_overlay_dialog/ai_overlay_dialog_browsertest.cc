// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ttc/core/switches.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ttc {

class AiOverlayDialogWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  AiOverlayDialogWebUIBrowserTest() {
    set_test_loader_host(chrome::kChromeUIAiOverlayDialogUntrustedHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ttc::switches::kEnableTtcTestApi);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
            features::kAiOverlayDialog},
        {});
    WebUIMochaBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest, Persona) {
  RunTest("ai_overlay_dialog/persona_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest, Conversation) {
  RunTest("ai_overlay_dialog/conversation_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest, ApiSession) {
  RunTest("ai_overlay_dialog/api_session_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest, App) {
  RunTest("ai_overlay_dialog/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest,
                       LocalStoragePersistsAcrossNavigations) {
  const GURL overlay_url(chrome::kChromeUIAiOverlayDialogUntrustedURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), overlay_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "window.localStorage.setItem('ttc_gemini_api_key', 'persisted-key');"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), overlay_url));

  EXPECT_EQ(
      "persisted-key",
      content::EvalJs(web_contents,
                      "window.localStorage.getItem('ttc_gemini_api_key');"));
}

}  // namespace ttc
