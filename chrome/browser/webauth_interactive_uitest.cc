// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class WebAuthFocusTest : public InProcessBrowserTest,
                         public AuthenticatorRequestDialogModel::Observer {
 protected:
  WebAuthFocusTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        permission_requested_(false) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_.Start());
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  bool permission_requested() { return permission_requested_; }

  AuthenticatorRequestDialogModel* dialog_model_;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override {
    if (dialog_model_->current_step() !=
        AuthenticatorRequestDialogModel::Step::kAttestationPermissionRequest)
      return;

    // Simulate accepting the permission request.
    dialog_model_->OnAttestationPermissionResponse(true);
    permission_requested_ = true;
  }

  void OnModelDestroyed() override {}

  net::EmbeddedTestServer https_server_;

  // Set to true when the permission sheet is triggered.
  bool permission_requested_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFocusTest);
};

IN_PROC_BROWSER_TEST_F(WebAuthFocusTest, Focus) {
  // Web Authentication requests will often trigger machine-wide indications,
  // such as a Security Key flashing for a touch. If background tabs were able
  // to trigger this, there would be a risk of user confusion since the user
  // would not know which tab they would be interacting with if they touched a
  // Security Key. Because of that, some Web Authentication APIs require that
  // the frame be in the foreground in a focused window.

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsURL("www.example.com", "/title1.html"));

  auto owned_virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  auto* virtual_device_factory = owned_virtual_device_factory.get();
  content::AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::move(owned_virtual_device_factory));

  constexpr char kRegisterTemplate[] =
      "navigator.credentials.create({publicKey: {"
      "  rp: {name: 't'},"
      "  user: {id: new Uint8Array([1]), name: 't', displayName: 't'},"
      "  challenge: new Uint8Array([1,2,3,4]),"
      "  timeout: 10000,"
      "  attestation: '$1',"
      "  pubKeyCredParams: [{type: 'public-key', alg: -7}]"
      "}}).then(c => window.domAutomationController.send('OK'),"
      "         e => window.domAutomationController.send(e.toString()));";
  const std::string register_script = base::ReplaceStringPlaceholders(
      kRegisterTemplate, std::vector<std::string>{"none"}, nullptr);

  content::WebContents* const initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string result;
  // When operating in the foreground, the operation should succeed.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_EQ(result, "OK");

  // Open a new tab to put the previous page in the background.
  chrome::NewTab(browser());

  // When in the background, the same request should result in a focus error.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  constexpr char kFocusErrorSubstring[] = "the page does not have focus";
  EXPECT_THAT(result, ::testing::HasSubstr(kFocusErrorSubstring));

  // Close the tab and the action should succeed again.
  chrome::CloseTab(browser());
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_EQ(result, "OK");

  // Start the request in the foreground and open a new tab between starting and
  // finishing the request. This should fail because we don't want foreground
  // pages to be able to start a request, open a trusted site in a new
  // tab/window, and have the user believe that they are interacting with that
  // trusted site.
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](Browser* browser, device::VirtualFidoDevice* device) {
            chrome::NewTab(browser);
            return true;
          },
          browser());
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_THAT(result, ::testing::HasSubstr(kFocusErrorSubstring));

  // Close the tab and the action should succeed again.
  chrome::CloseTab(browser());
  virtual_device_factory->mutable_state()->simulate_press_callback.Reset();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_EQ(result, "OK");

  // Open dev tools and check that operations still succeed.
  DevToolsWindow* dev_tools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          initial_web_contents, true /* docked, not a separate window */);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_EQ(result, "OK");
  DevToolsWindowTesting::CloseDevToolsWindowSync(dev_tools_window);

  // Open a second browser window.
  chrome::NewWindow(browser());
  Browser* new_window = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_window));

  // Operations in the (now unfocused) window should still succeed, as the
  // calling tab is still the active tab in that window.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_THAT(result, "OK");

  // Check that closing the window brings things back to a focused state.
  chrome::CloseWindow(new_window);
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(initial_web_contents,
                                                     register_script, &result));
  EXPECT_EQ(result, "OK");

  // Requesting "direct" attestation will trigger a permissions prompt.
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        dialog_model_ =
            AuthenticatorRequestScheduler::GetRequestDelegateForTest(
                initial_web_contents)
                ->WeakDialogModelForTesting();
        dialog_model_->AddObserver(this);
        return true;
      });

  const std::string get_assertion_with_attestation_script =
      base::ReplaceStringPlaceholders(
          kRegisterTemplate, std::vector<std::string>{"direct"}, nullptr);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      initial_web_contents, get_assertion_with_attestation_script, &result));

  EXPECT_TRUE(permission_requested());
  EXPECT_EQ(result, "OK");
}

}  // anonymous namespace
