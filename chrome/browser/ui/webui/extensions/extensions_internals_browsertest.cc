// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

using ExtensionsInternalsTest = extensions::ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionsInternalsTest,
                       TestExtensionsInternalsAreServed) {
  // Install an extension that we can check for.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  test_data_dir = test_data_dir.AppendASCII("extensions");
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  const extensions::Extension* extension =
      loader.LoadExtension(test_data_dir.AppendASCII("good.crx")).get();
  ASSERT_TRUE(extension);

  // First, check that navigation succeeds.
  GURL navigation_url(
      content::GetWebUIURL(chrome::kChromeUIExtensionsInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), navigation_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(navigation_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());

  // Look for a bit of JSON that has the extension's unique ID.
  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      base::StringPrintf("document.body.textContent && "
                                         "document.body.textContent.indexOf("
                                         "'\"id\": \"%s\"') >= 0;",
                                         extension->id().c_str())));
}
