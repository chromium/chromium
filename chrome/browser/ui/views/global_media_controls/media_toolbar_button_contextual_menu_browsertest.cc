// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include <memory>
#include <optional>

#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using MediaToolbarButtonContextualMenuBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MediaToolbarButtonContextualMenuBrowserTest,
                       ExecuteReportIssueCommand) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto menu = std::make_unique<MediaToolbarButtonContextualMenu>(
      browser()->GetProfile());
  auto model = menu->CreateMenuModel();
  std::optional<size_t> index =
      model->GetIndexOfCommandId(IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  ASSERT_TRUE(index.has_value());
  model->ActivatedAt(index.value(), 0);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            GURL("chrome://cast-feedback"));
#else
  GTEST_SKIP()
      << "Report issue command is only available in Google Chrome branding";
#endif
}
