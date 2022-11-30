// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// https://crbug.com/833624
class InvalidUserDataDirTest : public InProcessBrowserTest {
 public:
  InvalidUserDataDirTest() = default;
  InvalidUserDataDirTest(const InvalidUserDataDirTest&) = delete;
  InvalidUserDataDirTest& operator=(const InvalidUserDataDirTest&) = delete;
  ~InvalidUserDataDirTest() override = default;

 private:
  void SetUp() override {
    // Skip showing the error message box to avoid freezing the main thread.
    chrome::internal::g_should_skip_message_box_for_test = true;
    chrome::SetInvalidSpecifiedUserDataDir(
        base::FilePath(FILE_PATH_LITERAL("foo/bar/baz")));
    InProcessBrowserTest::SetUp();
  }

  // This override makes sure the screen instance is not set because in normal
  // browser initialization, the screen is not set until after the call to
  // chrome::GetInvalidSpecifiedUserDataDir.
  void SetScreenInstance() override {}
};

IN_PROC_BROWSER_TEST_F(InvalidUserDataDirTest, Basic) {
  // A message dialog may be showing which would block shutdown.
  chrome::Exit();
}
