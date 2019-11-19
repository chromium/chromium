// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/remoting/key_code_test_map.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/remote_test_helper.h"
#include "chrome/test/remoting/waiter.h"
#include "extensions/browser/app_window/app_window.h"

namespace remoting {

class Me2MeBrowserTest : public RemoteDesktopBrowserTest {
 protected:
  void TestKeypressInput(ui::KeyboardCode, const std::string&);

  void ConnectPinlessAndCleanupPairings(bool cleanup_all);
  bool IsPairingSpinnerHidden();
  void SetupForRemoteHostTest();

  void RestoreApp();
  void MinimizeApp();
};

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Connect_Local_Host \
  DISABLED_MANUAL_Me2Me_Connect_Local_Host
#else
#define MAYBE_MANUAL_Me2Me_Connect_Local_Host MANUAL_Me2Me_Connect_Local_Host
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Connect_Local_Host) {
  content::WebContents* content = SetUpTest();
  LoadScript(content, FILE_PATH_LITERAL("me2me_browser_test.js"));
  RunJavaScriptTest(content, "ConnectToLocalHost", "{"
    "pin: '" + me2me_pin() + "'"
  "}");

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Connect_Remote_Host \
  DISABLED_MANUAL_Me2Me_Connect_Remote_Host
#else
#define MAYBE_MANUAL_Me2Me_Connect_Remote_Host MANUAL_Me2Me_Connect_Remote_Host
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Connect_Remote_Host) {
  VerifyInternetAccess();
  Install();
  LaunchChromotingApp(false);

  // Authorize, Authenticate, and Approve.
  Auth();
  ExpandMe2Me();

  ConnectToRemoteHost(remote_host_name(), false);

  // TODO(weitaosu): Find a way to verify keyboard input injection.
  // We cannot use TestKeyboardInput because it assumes
  // that the client and the host are on the same machine.

  DisconnectMe2Me();
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Remote_Host_Keypress \
  DISABLED_MANUAL_Me2Me_Remote_Host_Keypress
#else
#define MAYBE_MANUAL_Me2Me_Remote_Host_Keypress \
  MANUAL_Me2Me_Remote_Host_Keypress
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Remote_Host_Keypress) {
  SetupForRemoteHostTest();

  // Test all key characters
  int length = sizeof(test_alpha_map)/sizeof(KeyCodeTestMap);
  for (int i = 0; i < length; i++) {
    KeyCodeTestMap key = test_alpha_map[i];
    TestKeypressInput(key.vkey_code, key.code);
  }
  DisconnectMe2Me();
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Remote_Host_Digitpress \
  DISABLED_MANUAL_Me2Me_Remote_Host_Digitpress
#else
#define MAYBE_MANUAL_Me2Me_Remote_Host_Digitpress \
  MANUAL_Me2Me_Remote_Host_Digitpress
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Remote_Host_Digitpress) {
  SetupForRemoteHostTest();

  // Test all digit characters
  int length = sizeof(test_digit_map)/sizeof(KeyCodeTestMap);
  for (int i = 0; i < length; i++) {
    KeyCodeTestMap key = test_digit_map[i];
    TestKeypressInput(key.vkey_code, key.code);
  }
  DisconnectMe2Me();
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Remote_Host_Specialpress \
  DISABLED_MANUAL_Me2Me_Remote_Host_Specialpress
#else
#define MAYBE_MANUAL_Me2Me_Remote_Host_Specialpress \
  MANUAL_Me2Me_Remote_Host_Specialpress
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Remote_Host_Specialpress) {
  SetupForRemoteHostTest();

  // Test all special characters
  int length = sizeof(test_special_map)/sizeof(KeyCodeTestMap);
  for (int i = 0; i < length; i++) {
    KeyCodeTestMap key = test_special_map[i];
    TestKeypressInput(key.vkey_code, key.code);
  }
  DisconnectMe2Me();
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Remote_Host_Numpadpress \
  DISABLED_MANUAL_Me2Me_Remote_Host_Numpadpress
#else
#define MAYBE_MANUAL_Me2Me_Remote_Host_Numpadpress \
  MANUAL_Me2Me_Remote_Host_Numpadpress
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Remote_Host_Numpadpress) {
  SetupForRemoteHostTest();

  // Test all numpad characters
  int length = sizeof(test_numpad_map)/sizeof(KeyCodeTestMap);
  for (int i = 0; i < length; i++) {
    KeyCodeTestMap key = test_numpad_map[i];
    TestKeypressInput(key.vkey_code, key.code);
  }
  DisconnectMe2Me();
  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Connect_Pinless DISABLED_MANUAL_Me2Me_Connect_Pinless
#else
#define MAYBE_MANUAL_Me2Me_Connect_Pinless MANUAL_Me2Me_Connect_Pinless
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest, MAYBE_MANUAL_Me2Me_Connect_Pinless) {
  SetUpTest();

  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-message"))
      << "The host must have no pairings before running the pinless test.";

  // Test that cleanup works with either the Delete or Delete all buttons.
  ConnectPinlessAndCleanupPairings(false);
  ConnectPinlessAndCleanupPairings(true);

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_v2_Alive_OnLostFocus \
  DISABLED_MANUAL_Me2Me_v2_Alive_OnLostFocus
#else
#define MAYBE_MANUAL_Me2Me_v2_Alive_OnLostFocus \
  MANUAL_Me2Me_v2_Alive_OnLostFocus
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_v2_Alive_OnLostFocus) {
  content::WebContents* content = SetUpTest();
  LoadScript(content, FILE_PATH_LITERAL("me2me_browser_test.js"));
  RunJavaScriptTest(content, "AliveOnLostFocus", "{"
    "pin: '" + me2me_pin() + "'"
  "}");

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Me2Me_Disable_Remote_Connection \
  DISABLED_MANUAL_Me2Me_Disable_Remote_Connection
#else
#define MAYBE_MANUAL_Me2Me_Disable_Remote_Connection \
  MANUAL_Me2Me_Disable_Remote_Connection
#endif
IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MAYBE_MANUAL_Me2Me_Disable_Remote_Connection) {
  SetUpTest();

  DisableRemoteConnection();
  EXPECT_FALSE(IsLocalHostReady());

  Cleanup();
}

void Me2MeBrowserTest::SetupForRemoteHostTest() {
  VerifyInternetAccess();
  OpenClientBrowserPage();
  Install();
  LaunchChromotingApp(false);

  // Authorize, Authenticate, and Approve.
  Auth();
  ExpandMe2Me();
  ConnectToRemoteHost(remote_host_name(), false);

  // Wake up the machine if it's sleeping.
  // This is only needed when testing manually as the host machine
  // may be sleeping.
  SimulateKeyPressWithCode(ui::VKEY_RETURN, "Enter");
}

void Me2MeBrowserTest::TestKeypressInput(
    ui::KeyboardCode keyCode,
    const std::string& code) {
  remote_test_helper()->ClearLastEvent();
  VLOG(1) << "Pressing " << code;
  SimulateKeyPressWithCode(keyCode, code);
  Event event;
  remote_test_helper()->GetLastEvent(&event);
  ASSERT_EQ(Action::Keydown, event.action);
  ASSERT_EQ(keyCode, event.value);
}

void Me2MeBrowserTest::ConnectPinlessAndCleanupPairings(bool cleanup_all) {
  // First connection: verify that a PIN is requested, and request pairing.
  ConnectToLocalHost(true);
  DisconnectMe2Me();

  // TODO(jamiewalch): This reload is only needed because there's a bug in the
  // web-app whereby it doesn't refresh its pairing state correctly.
  // http://crbug.com/311290
  LaunchChromotingApp(false);
  ASSERT_TRUE(HtmlElementVisible("paired-client-manager-message"));

  // Second connection: verify that no PIN is requested.
  ClickOnControl("this-host-connect");
  WaitForConnection();
  DisconnectMe2Me();

  // Clean up pairings.
  ClickOnControl("open-paired-client-manager-dialog");
  ASSERT_TRUE(HtmlElementVisible("paired-client-manager-dialog"));

  if (cleanup_all) {
    ClickOnControl("delete-all-paired-clients");
  } else {
    std::string host_id = ExecuteScriptAndExtractString(
        "remoting.pairedClientManager.getFirstClientIdForTesting_()");
    std::string node_id = "delete-client-" + host_id;
    ClickOnControl(node_id);
  }

  // Wait for the "working" spinner to disappear. The spinner is shown by both
  // methods of deleting a host and is removed when the operation completes.
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(5),
      base::TimeDelta::FromMilliseconds(200),
      base::Bind(&Me2MeBrowserTest::IsPairingSpinnerHidden,
                 base::Unretained(this)));
  EXPECT_TRUE(waiter.Wait());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "document.getElementById('delete-all-paired-clients').disabled"));

  ClickOnControl("close-paired-client-manager-dialog");
  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-dialog"));
  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-message"));
}

bool Me2MeBrowserTest::IsPairingSpinnerHidden() {
  return !HtmlElementVisible("paired-client-manager-dialog-working");
}

}  // namespace remoting
