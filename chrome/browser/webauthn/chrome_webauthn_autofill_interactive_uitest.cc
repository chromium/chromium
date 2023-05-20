// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

static constexpr uint8_t kCredentialID1[] = {1, 2, 3, 4};
static constexpr uint8_t kCredentialID2[] = {2, 3, 4, 5};

static constexpr char kConditionalUIRequest[] = R"((() => {
window.requestAbortController = new AbortController();
navigator.credentials.get({
  signal: window.requestAbortController.signal,
  mediation: 'conditional',
  publicKey: {
    challenge: new Uint8Array([1,2,3,4]),
    timeout: 10000,
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kConditionalUIRequestFiltered[] = R"((() => {
  window.requestAbortController = new AbortController();
  let cred_id = new Uint8Array([1,2,3,4]);
  navigator.credentials.get({
    signal: window.requestAbortController.signal,
    mediation: 'conditional',
    publicKey: {
      challenge: cred_id,
      timeout: 10000,
      userVerification: 'discouraged',
      allowCredentials: [{type: 'public-key', id: cred_id}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

// Autofill integration tests. This file contains end-to-end tests for
// integration between WebAuthn and Autofill. These tests are sensitive to focus
// changes, so they are interactive UI tests.

// Base class for autofill integration tests, contains the actual test code but
// no setup.
class WebAuthnAutofillIntegrationTest : public CertVerifierBrowserTest {
 public:
  WebAuthnAutofillIntegrationTest() = default;

  WebAuthnAutofillIntegrationTest(const WebAuthnAutofillIntegrationTest&) =
      delete;
  WebAuthnAutofillIntegrationTest& operator=(
      const WebAuthnAutofillIntegrationTest&) = delete;

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

    // Save a credential to the password store. This will let us wait on the
    // popup to appear after aborting the request.
    password_manager::PasswordStoreInterface* password_store =
        PasswordStoreFactory::GetForProfile(browser()->profile(),
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    password_manager::PasswordForm signin_form;
    GURL url = https_server_.GetURL("www.example.com", "/");
    signin_form.signon_realm = url.spec();
    signin_form.url = url;
    signin_form.action = url;
    signin_form.username_value = u"remilia";
    signin_form.password_value = u"shouldbeusingapasskeyinstead";
    base::RunLoop run_loop;
    password_store->AddLogin(signin_form, run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_server_.GetURL("www.example.com",
                             "/webauthn_conditional_mediation.html")));
  }

  void RunSelectAccountTest(const char* request) {
    // Make sure input events cannot close the autofill popup.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents);
    autofill_client->KeepPopupOpenForTesting();

    // Execute the Conditional UI request.
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, request);

    // Interact with the username field until the popup shows up. This has the
    // effect of waiting for the browser to send the renderer the password
    // information, and waiting for the UI to render.
    base::WeakPtr<autofill::AutofillPopupController> popup_controller;
    while (!popup_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      popup_controller = autofill_client->popup_controller_for_testing();
    }

    // Find the webauthn credential on the suggestions list.
    auto suggestions = popup_controller->GetSuggestions();
    size_t suggestion_index = 0;
    size_t webauthn_entry_count = 0;
    autofill::Suggestion webauthn_entry;
    for (size_t i = 0; i < suggestions.size(); ++i) {
      if (suggestions[i].frontend_id ==
          autofill::PopupItemId::kWebauthnCredential) {
        webauthn_entry = suggestions[i];
        suggestion_index = i;
        webauthn_entry_count++;
      }
    }
    ASSERT_EQ(webauthn_entry_count, 1u);
    ASSERT_LT(suggestion_index, suggestions.size())
        << "WebAuthn entry not found";
    EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
    EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value, GetDeviceString());
    EXPECT_EQ(webauthn_entry.icon, "globeIcon");

    // Click the credential.
    popup_controller->AcceptSuggestionWithoutThreshold(suggestion_index);
    std::string result;
    ASSERT_TRUE(message_queue.WaitForMessage(&result));
    EXPECT_EQ(result, "\"webauthn: OK\"");
  }

  void RunAbortTest() {
    // Make sure input events cannot close the autofill popup.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents);
    autofill_client->KeepPopupOpenForTesting();

    // Execute the Conditional UI request.
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);

    // Interact with the username field until the popup shows up. This has the
    // effect of waiting for the browser to send the renderer the password
    // information, and waiting for the UI to render.
    base::WeakPtr<autofill::AutofillPopupController> popup_controller;
    while (!popup_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      popup_controller = autofill_client->popup_controller_for_testing();
    }

    // Find the webauthn credential on the suggestions list.
    auto suggestions = popup_controller->GetSuggestions();
    size_t suggestion_index;
    autofill::Suggestion webauthn_entry;
    for (suggestion_index = 0; suggestion_index < suggestions.size();
         ++suggestion_index) {
      if (suggestions[suggestion_index].frontend_id ==
          autofill::PopupItemId::kWebauthnCredential) {
        webauthn_entry = suggestions[suggestion_index];
        break;
      }
    }
    ASSERT_LT(suggestion_index, suggestions.size())
        << "WebAuthn entry not found";
    EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
    EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value, GetDeviceString());
    EXPECT_EQ(webauthn_entry.icon, "globeIcon");

    // Abort the request.
    content::ExecuteScriptAsync(web_contents,
                                "window.requestAbortController.abort()");
    std::string result;
    ASSERT_TRUE(message_queue.WaitForMessage(&result));
    EXPECT_EQ(result, "\"error AbortError: signal is aborted without reason\"");

    // The popup may have gone away while waiting. If not, make sure it's gone.
    if (popup_controller) {
      popup_controller->Hide(autofill::PopupHidingReason::kUserAborted);
    }

    // Interact with the username field. Since there is still a saved password,
    // the popup should eventually show up.
    while (!popup_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      popup_controller = autofill_client->popup_controller_for_testing();
    }
    for (const auto& suggestion : popup_controller->GetSuggestions()) {
      EXPECT_NE(suggestion.frontend_id,
                autofill::PopupItemId::kWebauthnCredential);
      EXPECT_NE(suggestion.frontend_id,
                autofill::PopupItemId::kWebauthnSignInWithAnotherDevice);
    }
  }

  virtual std::u16string GetDeviceString() = 0;

  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Autofill integration test using the devtools virtual environment.
class WebAuthnDevtoolsAutofillIntegrationTest
    : public WebAuthnAutofillIntegrationTest {
 public:
  void SetUpOnMainThread() override {
    WebAuthnAutofillIntegrationTest::SetUpOnMainThread();

    // Set up a fake virtual device.
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory->SetTransport(
        device::FidoTransportProtocol::kInternal);
    virtual_device_factory_ = virtual_device_factory.get();
    virtual_device_factory->mutable_state()->InjectResidentKey(
        kCredentialID1, "www.example.com", std::vector<uint8_t>{5, 6, 7, 8},
        "flandre", "Flandre Scarlet");
    virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    config.internal_uv_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));
    scoped_auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's, these objects need to be destroyed before
    // the test class.
    virtual_device_factory_ = nullptr;
    scoped_auth_env_.reset();
    WebAuthnAutofillIntegrationTest::PostRunTestOnMainThread();
  }

  std::u16string GetDeviceString() override {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE);
  }

 protected:
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting>
      scoped_auth_env_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest, SelectAccount) {
  RunSelectAccountTest(kConditionalUIRequest);
}

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest, Abort) {
  RunAbortTest();
}

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest,
                       SelectAccountWithAllowCredentials) {
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest,
                       SelectAccountWithAllowCredentialsFiltered) {
  virtual_device_factory_->mutable_state()->InjectResidentKey(
      kCredentialID2, "www.example.com", std::vector<uint8_t>{6, 7, 8, 9},
      "sakuya", "Sakuya Izayoi");
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

#if BUILDFLAG(IS_WIN)
// Autofill integration test using the Windows fake API.
class WebAuthnWindowsAutofillIntegrationTest
    : public WebAuthnAutofillIntegrationTest {
 public:
  void SetUpOnMainThread() override {
    WebAuthnAutofillIntegrationTest::SetUpOnMainThread();

    // Set up the fake Windows platform authenticator.
    fake_webauthn_api_ = std::make_unique<device::FakeWinWebAuthnApi>();
    fake_webauthn_api_->set_version(WEBAUTHN_API_VERSION_4);
    fake_webauthn_api_->set_is_uvpaa(true);
    fake_webauthn_api_->set_supports_silent_discovery(true);
    device::PublicKeyCredentialUserEntity user({1, 2, 3, 4}, "flandre",
                                               "Flandre Scarlet");
    device::PublicKeyCredentialRpEntity rp("www.example.com");
    fake_webauthn_api_->InjectDiscoverableCredential(
        kCredentialID1, std::move(rp), std::move(user));

    // Inject the fake Windows platform authenticator.
    auto device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    device_factory->set_win_webauthn_api(fake_webauthn_api_.get());
    scoped_auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(device_factory));
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's, these objects need to be destroyed before
    // the test class.
    virtual_device_factory_ = nullptr;
    scoped_auth_env_.reset();
    WebAuthnAutofillIntegrationTest::PostRunTestOnMainThread();
  }

  std::u16string GetDeviceString() override {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_WINDOWS_HELLO);
  }

 protected:
  std::unique_ptr<device::FakeWinWebAuthnApi> fake_webauthn_api_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting>
      scoped_auth_env_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnWindowsAutofillIntegrationTest, SelectAccount) {
  RunSelectAccountTest(kConditionalUIRequest);
}

IN_PROC_BROWSER_TEST_F(WebAuthnWindowsAutofillIntegrationTest,
                       SelectAccountWithAllowCredentials) {
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

IN_PROC_BROWSER_TEST_F(WebAuthnWindowsAutofillIntegrationTest,
                       SelectAccountWithAllowCredentialsFiltered) {
  device::PublicKeyCredentialUserEntity user({6, 7, 8, 9}, "sakuya",
                                             "Sakuya Izayoi");
  device::PublicKeyCredentialRpEntity rp("www.example.com");
  fake_webauthn_api_->InjectDiscoverableCredential(
      kCredentialID2, std::move(rp), std::move(user));
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

IN_PROC_BROWSER_TEST_F(WebAuthnWindowsAutofillIntegrationTest, Abort) {
  RunAbortTest();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
