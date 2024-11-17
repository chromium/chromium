// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_dangerous_file_dialog.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class FileSystemAccessDangerousFileDialogBrowserTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowFileSystemAccessDangerousFileDialog(
        url::Origin::Create(GURL("https://example.com")),
        content::PathInfo(FILE_PATH_LITERAL("bar.swf")), base::DoNothing(),
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessDangerousFileDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
