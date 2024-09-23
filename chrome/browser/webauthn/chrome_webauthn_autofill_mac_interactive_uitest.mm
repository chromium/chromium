// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static constexpr char kConditionalUIRequest[] = R"((() => {
navigator.credentials.get({
  mediation: 'conditional',
  publicKey: {
    challenge: new Uint8Array([1,2,3,4]),
    timeout: 10000,
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

class WebAuthnMacAutofillIntegrationTest : public CertVerifierBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    CertVerifierBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Allowlist all certs for the HTTPS server.
    auto cert = https_server_.GetCertificate();
    net::CertVerifyResult verify_result;
    verify_result.cert_status = 0;
    verify_result.verified_cert = cert;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_server_.GetURL("www.example.com",
                             "/webauthn_conditional_mediation.html")));

    // Set up the fake keychain.
    config_ =
        ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
            browser()->profile());
    touch_id_test_environment_ =
        std::make_unique<device::fido::mac::ScopedTouchIdTestEnvironment>(
            config_);
    store_ =
        std::make_unique<device::fido::mac::TouchIdCredentialStore>(config_);
    device::PublicKeyCredentialUserEntity user({1, 2, 3, 4}, "flandre",
                                               "Flandre Scarlet");
    store_->CreateCredential(
        "www.example.com", std::move(user),
        device::fido::mac::TouchIdCredentialStore::kDiscoverable);
    touch_id_test_environment_->SimulateTouchIdPromptSuccess();
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  device::fido::mac::AuthenticatorConfig config_;
  std::unique_ptr<device::fido::mac::ScopedTouchIdTestEnvironment>
      touch_id_test_environment_;
  std::unique_ptr<device::fido::mac::TouchIdCredentialStore> store_;
};

// Integration test between the mac keychain platform authenticator and autofill
// UI.
IN_PROC_BROWSER_TEST_F(WebAuthnMacAutofillIntegrationTest, SelectAccount) {
  // Make sure input events cannot close the autofill popup.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  autofill::ChromeAutofillClient* autofill_client =
      autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents);
  autofill_client->SetKeepPopupOpenForTesting(true);

  // Execute the Conditional UI request.
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);

  // Interact with the username field until the popup shows up. This has the
  // effect of waiting for the browser to send the renderer the password
  // information, and waiting for the UI to render.
  base::WeakPtr<autofill::AutofillSuggestionController> controller;
  while (!controller) {
    content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
    controller = autofill_client->suggestion_controller_for_testing();
  }

  auto suggestions = controller->GetSuggestions();
  size_t suggestion_index;
  autofill::Suggestion webauthn_entry;
  for (suggestion_index = 0; suggestion_index < suggestions.size();
       ++suggestion_index) {
    if (suggestions[suggestion_index].type ==
        autofill::SuggestionType::kWebauthnCredential) {
      webauthn_entry = suggestions[suggestion_index];
      break;
    }
  }
  ASSERT_LT(suggestion_index, suggestions.size()) << "WebAuthn entry not found";
  EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
  EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value,
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_PASSKEY_FROM_CHROME_PROFILE));
  EXPECT_EQ(webauthn_entry.icon, autofill::Suggestion::Icon::kGlobe);

  // Click the credential.
  test_api(static_cast<autofill::AutofillPopupControllerImpl&>(*controller))
      .DisableThreshold(true);
  controller->AcceptSuggestion(suggestion_index);
  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");
}

}  // namespace
