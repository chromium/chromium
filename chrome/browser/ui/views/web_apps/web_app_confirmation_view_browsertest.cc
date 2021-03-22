// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using WebAppConfirmViewBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppConfirmViewBrowserTest, ShowWebAppInstallDialog) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = u"Test app";
  app_info->start_url = GURL("https://example.com");

  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/true);
  bool is_accepted = false;
  auto callback = [&is_accepted](bool result,
                                 std::unique_ptr<WebApplicationInfo>) {
    is_accepted = result;
  };

  chrome::ShowWebAppInstallDialog(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(callback));
  EXPECT_TRUE(is_accepted);
}
