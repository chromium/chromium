// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using SignoutConfirmationBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SignoutConfirmationBrowserTest,
                       DirectNavigationDoesNotCrashOrInitializeHandler) {
  SignoutConfirmationUIConfig config;
  EXPECT_TRUE(config.IsWebUIEnabled(GetProfile()));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, GURL(chrome::kChromeUISignoutConfirmationURL)));

  SignoutConfirmationUI* signout_confirmation_ui =
      SignoutConfirmationUI::GetForTesting(web_contents);
  ASSERT_TRUE(signout_confirmation_ui);

  EXPECT_EQ(
      true,
      content::EvalJs(
          web_contents,
          "(async () => {"
          "  await customElements.whenDefined('signout-confirmation-app');"
          "  const {SignoutConfirmationBrowserProxyImpl} ="
          "      await import('./signout_confirmation.js');"
          "  const remote ="
          "      SignoutConfirmationBrowserProxyImpl.getInstance().handler.$;"
          "  await remote.flushForTesting().catch(() => {});"
          "  return document.querySelector('signout-confirmation-app')"
          "      "
          ".shadowRoot.querySelector('#signoutConfirmationDialog').hidden;"
          "})()"));
  EXPECT_FALSE(signout_confirmation_ui->IsHandlerReadyForTesting());
}
