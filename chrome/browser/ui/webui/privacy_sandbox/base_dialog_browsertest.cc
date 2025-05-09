// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/privacy_sandbox/dialog_view_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using PrivacySandboxBaseDialogTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PrivacySandboxBaseDialogTest, PageLoads) {
  GURL kUrl(chrome::kChromeUIPrivacySandboxBaseDialogURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Verify the marker is absent when the URL is loaded directly in a tab, as
  // opposed to being instantiated via the dialog view.
  EXPECT_EQ(privacy_sandbox::DialogViewContext::FromWebContents(web_contents),
            nullptr);
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kUrl);
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_EQ(web_contents->GetTitle(),
            l10n_util::GetStringUTF16(IDS_SETTINGS_AD_PRIVACY_PAGE_TITLE));
}

}  // namespace
