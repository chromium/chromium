// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class NotificationsCustomBindingsBrowserTest : public InProcessBrowserTest {
 public:
  NotificationsCustomBindingsBrowserTest(
      const NotificationsCustomBindingsBrowserTest&) = delete;
  NotificationsCustomBindingsBrowserTest& operator=(
      const NotificationsCustomBindingsBrowserTest&) = delete;
  ~NotificationsCustomBindingsBrowserTest() override = default;

 protected:
  NotificationsCustomBindingsBrowserTest() = default;

  virtual void RunTest(const std::string& trigger) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_);
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    LoadLibrary(
        "chrome/renderer/resources/extensions/notifications_test_util.js",
        web_contents);
    LoadLibrary(
        "chrome/renderer/resources/extensions/notifications_custom_bindings.js",
        web_contents);
    LoadLibrary(
        "chrome/renderer/resources/extensions/"
        "notifications_custom_bindings_test.js",
        web_contents);

    ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), trigger));
  }

 private:
  void LoadLibrary(const std::string& path,
                   content::WebContents* web_contents) {
    base::FilePath full_path =
        base::MakeAbsoluteFilePath(src_root_.AppendASCII(path));
    std::string library_content;
    if (!base::ReadFileToString(full_path, &library_content)) {
      FAIL() << "Failed reading " << path;
    }
    ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), library_content));
  }

  base::FilePath src_root_;
};

IN_PROC_BROWSER_TEST_F(NotificationsCustomBindingsBrowserTest,
                       TestImageDataSetter) {
  RunTest("testImageDataSetter()");
}

IN_PROC_BROWSER_TEST_F(NotificationsCustomBindingsBrowserTest,
                       TestGetUrlSpecs) {
  RunTest("testGetUrlSpecs()");
}

IN_PROC_BROWSER_TEST_F(NotificationsCustomBindingsBrowserTest,
                       TestGetUrlSpecsScaled) {
  RunTest("testGetUrlSpecsScaled()");
}
