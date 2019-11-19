// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/remote_test_helper.h"

namespace remoting {

class It2MeBrowserTest : public RemoteDesktopBrowserTest {
 protected:
  std::string GetAccessCode(content::WebContents* contents);

  // Launches a Chromoting app instance for the helper.
  content::WebContents* SetUpHelperInstance();
};

std::string It2MeBrowserTest::GetAccessCode(content::WebContents* contents) {
  RunJavaScriptTest(contents, "GetAccessCode", "{}");
  std::string access_code = RemoteTestHelper::ExecuteScriptAndExtractString(
      contents, "document.getElementById('access-code-display').innerText");
  return access_code;
}

content::WebContents* It2MeBrowserTest::SetUpHelperInstance() {
  content::WebContents* helper_content =
      LaunchChromotingApp(false, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  LoadBrowserTestJavaScript(helper_content);
  LoadScript(helper_content, FILE_PATH_LITERAL("it2me_browser_test.js"));
  return helper_content;
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Connect DISABLED_MANUAL_Connect
#else
#define MAYBE_MANUAL_Connect MANUAL_Connect
#endif
IN_PROC_BROWSER_TEST_F(It2MeBrowserTest, MAYBE_MANUAL_Connect) {
  content::WebContents* helpee_content = SetUpTest();
  LoadScript(helpee_content, FILE_PATH_LITERAL("it2me_browser_test.js"));

  content::WebContents* helper_content = SetUpHelperInstance();
  RunJavaScriptTest(helper_content, "ConnectIt2Me", "{"
    "accessCode: '" + GetAccessCode(helpee_content) + "'"
  "}");

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_CancelShare DISABLED_MANUAL_CancelShare
#else
#define MAYBE_MANUAL_CancelShare MANUAL_CancelShare
#endif
IN_PROC_BROWSER_TEST_F(It2MeBrowserTest, MAYBE_MANUAL_CancelShare) {
  content::WebContents* helpee_content = SetUpTest();
  LoadScript(helpee_content, FILE_PATH_LITERAL("it2me_browser_test.js"));
  std::string access_code = GetAccessCode(helpee_content);
  RunJavaScriptTest(helpee_content, "CancelShare", "{}");

  content::WebContents* helper_content = SetUpHelperInstance();
  RunJavaScriptTest(helper_content, "InvalidAccessCode", "{"
    "accessCode: '" + access_code + "'"
  "}");
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_VerifyAccessCodeNonReusable \
  DISABLED_MANUAL_VerifyAccessCodeNonReusable
#else
#define MAYBE_MANUAL_VerifyAccessCodeNonReusable \
  MANUAL_VerifyAccessCodeNonReusable
#endif
IN_PROC_BROWSER_TEST_F(It2MeBrowserTest,
                       MAYBE_MANUAL_VerifyAccessCodeNonReusable) {
  content::WebContents* helpee_content = SetUpTest();
  LoadScript(helpee_content, FILE_PATH_LITERAL("it2me_browser_test.js"));
  std::string access_code = GetAccessCode(helpee_content);

  content::WebContents* helper_content = SetUpHelperInstance();
  RunJavaScriptTest(helper_content, "ConnectIt2Me", "{"
    "accessCode: '" + access_code + "'"
  "}");

  RunJavaScriptTest(helper_content, "InvalidAccessCode", "{"
    "accessCode: '" + access_code + "'"
  "}");
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_InvalidAccessCode DISABLED_MANUAL_InvalidAccessCode
#else
#define MAYBE_MANUAL_InvalidAccessCode MANUAL_InvalidAccessCode
#endif
IN_PROC_BROWSER_TEST_F(It2MeBrowserTest, MAYBE_MANUAL_InvalidAccessCode) {
  content::WebContents* helpee_content = SetUpTest();
  LoadScript(helpee_content, FILE_PATH_LITERAL("it2me_browser_test.js"));

  // Generate an invalid access code by generating a valid access code and
  // changing its PIN portion.
  std::string access_code = GetAccessCode(helpee_content);

  uint64_t invalid_access_code = 0;
  ASSERT_TRUE(base::StringToUint64(access_code, &invalid_access_code));
  std::ostringstream invalid_access_code_string;

  invalid_access_code_string << ++invalid_access_code;

  content::WebContents* helper_content = SetUpHelperInstance();
  RunJavaScriptTest(helper_content, "InvalidAccessCode", "{"
    "accessCode: '" + invalid_access_code_string.str() + "'"
  "}");

  Cleanup();
}

}  // namespace remoting
