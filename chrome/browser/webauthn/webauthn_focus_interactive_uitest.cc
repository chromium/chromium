// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

constexpr char kRegisterTemplate[] =
    "() => {"
    "  return navigator.credentials.create({publicKey: {"
    "    rp: {name: 't'},"
    "    user: {id: new Uint8Array([1]), name: 't', displayName: 't'},"
    "    challenge: new Uint8Array([1,2,3,4]),"
    "    timeout: 10000,"
    "    attestation: '$1',"
    "    pubKeyCredParams: [{type: 'public-key', alg: -7}]"
    "  }}).then(c => {return 'OK';},"
    "           e => {return e.toString();})"
    "}";

constexpr char kOkText[] = "OK";
constexpr char kNotAllowedText[] =
    "NotAllowedError: The operation is not allowed at this time because the "
    "page does not have focus.";

class WebAuthnFocusTest : public InteractiveBrowserTest,
                          public AuthenticatorRequestDialogModel::Observer {
 public:
  WebAuthnFocusTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  WebAuthnFocusTest(const WebAuthnFocusTest&) = delete;
  WebAuthnFocusTest& operator=(const WebAuthnFocusTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_.Start());
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  device::test::VirtualFidoDeviceFactory* virtual_device_factory() {
    return virtual_device_factory_;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {}

  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;

  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnFocusTest, SucceedWhenTabIsFocused) {
  RunTestSequence(
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GetHttpsURL("www.example.com", "/title1.html")),
      CheckJsResult(
          kFirstTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kOkText));
}

IN_PROC_BROWSER_TEST_F(WebAuthnFocusTest, FailWhenTabIsNotFocused) {
  // Web Authentication requests will often trigger machine-wide indications,
  // such as a Security Key flashing for a touch. If background tabs were able
  // to trigger this, there would be a risk of user confusion since the user
  // would not know which tab they would be interacting with if they touched a
  // Security Key. Because of that, some Web Authentication APIs require that
  // the frame be in the foreground in a focused window.
  RunTestSequence(
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GetHttpsURL("www.example.com", "/title1.html")),
      AddInstrumentedTab(kSecondTabId,
                         GetHttpsURL("www.example.com", "/title1.html")),
      CheckJsResult(
          kFirstTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kNotAllowedText),
      CheckJsResult(
          kSecondTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kOkText));
}

IN_PROC_BROWSER_TEST_F(WebAuthnFocusTest, FailWhenTabFocusChangesWhileRunning) {
  // Start the request in the foreground and open a new tab between starting and
  // finishing the request. This should fail because we don't want foreground
  // pages to be able to start a request, open a trusted site in a new
  // tab/window, and have the user believe that they are interacting with that
  // trusted site.
  virtual_device_factory()->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](Browser* browser, device::VirtualFidoDevice* device) {
            chrome::NewTab(browser);
            return true;
          },
          browser());
  RunTestSequence(
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GetHttpsURL("www.example.com", "/title1.html")),
      CheckJsResult(
          kFirstTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kNotAllowedText));
}

IN_PROC_BROWSER_TEST_F(WebAuthnFocusTest, SucceedWithDevtoolsOpen) {
  RunTestSequence(
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GetHttpsURL("www.example.com", "/title1.html")),
      FocusWebContents(kFirstTabId),
      CheckJsResult(kFirstTabId, "() => document.hasFocus()", true),
      Do([this]() {
        DevToolsWindowTesting::OpenDevToolsWindowSync(
            browser()->tab_strip_model()->GetActiveWebContents(),
            true /* docked, not a separate window */);
      }),
      CheckJsResult(kFirstTabId, "() => document.hasFocus()", false),
      CheckJsResult(
          kFirstTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kOkText));
}

IN_PROC_BROWSER_TEST_F(WebAuthnFocusTest, SucceedWithNewWindowOpen) {
  virtual_device_factory()->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](Browser* browser, device::VirtualFidoDevice* device) {
            chrome::NewWindow(browser);
            Browser* new_window = BrowserList::GetInstance()->GetLastActive();
            return ui_test_utils::BringBrowserWindowToFront(new_window);
          },
          browser());
  RunTestSequence(
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GetHttpsURL("www.example.com", "/title1.html")),
      CheckJsResult(
          kFirstTabId,
          base::ReplaceStringPlaceholders(
              kRegisterTemplate, std::vector<std::string>{"none"}, nullptr),
          kOkText));
}

}  // anonymous namespace
