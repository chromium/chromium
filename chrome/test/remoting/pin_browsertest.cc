// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/waiter.h"

namespace remoting {

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Cancel_PIN DISABLED_MANUAL_Cancel_PIN
#else
#define MAYBE_MANUAL_Cancel_PIN MANUAL_Cancel_PIN
#endif
IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MAYBE_MANUAL_Cancel_PIN) {
  content::WebContents* content = SetUpTest();
  LoadScript(content, FILE_PATH_LITERAL("cancel_pin_browser_test.js"));

  RunJavaScriptTest(content, "Cancel_PIN", "{"
    "pin: '" + me2me_pin() + "'"
  "}");

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Invalid_PIN DISABLED_MANUAL_Invalid_PIN
#else
#define MAYBE_MANUAL_Invalid_PIN MANUAL_Invalid_PIN
#endif
IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MAYBE_MANUAL_Invalid_PIN) {
  content::WebContents* content = SetUpTest();
  LoadScript(content, FILE_PATH_LITERAL("invalid_pin_browser_test.js"));

  RunJavaScriptTest(content, "Invalid_PIN", "{"
    // Append arbitrary characters after the pin to make it invalid.
    "pin: '" + me2me_pin() + "ABC'"
  "}");

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Update_PIN DISABLED_MANUAL_Update_PIN
#else
#define MAYBE_MANUAL_Update_PIN MANUAL_Update_PIN
#endif
IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MAYBE_MANUAL_Update_PIN) {
  content::WebContents* content = SetUpTest();
  LoadScript(content, FILE_PATH_LITERAL("update_pin_browser_test.js"));

  RunJavaScriptTest(content, "Update_PIN", "{"
    "old_pin: '" + me2me_pin() + "',"
    "new_pin: '314159'"
  "}");

  Cleanup();
}

}  // namespace remoting
