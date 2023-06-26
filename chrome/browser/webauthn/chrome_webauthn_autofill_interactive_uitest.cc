// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

static constexpr char kRpId[] = "example.com";
static constexpr uint8_t kCredentialID1[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                             9, 10, 11, 12, 13, 14, 15, 16};
static constexpr uint8_t kCredentialID2[] = {2, 3, 4, 5};
static constexpr char16_t kPhoneName[] = u"Flandre's Pixel 7";

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
  let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
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

sync_pb::WebauthnCredentialSpecifics CreatePasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(kCredentialID1, 16);
  passkey.set_rp_id(kRpId);
  passkey.set_user_id({1, 2, 3, 4});
  passkey.set_user_name("flandre");
  passkey.set_user_display_name("Flandre Scarlet");
  return passkey;
}

syncer::DeviceInfo CreateDeviceInfo() {
  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.contact_id = std::vector<uint8_t>({1, 2, 3});
  base::ranges::fill(paask_info.peer_public_key_x962, 0);
  paask_info.peer_public_key_x962[0] = 1;
  base::ranges::fill(paask_info.secret, 0);
  paask_info.secret[0] = 2;
  paask_info.id = device::cablev2::sync::IDNow();
  paask_info.tunnel_server_domain = 0;
  return syncer::DeviceInfo(
      /*guid=*/"guid",
      /*client_name=*/base::UTF16ToUTF8(kPhoneName),
      /*chrome_version=*/"chrome_version",
      /*sync_user_agent=*/"sync_user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      syncer::DeviceInfo::OsType::kLinux,
      syncer::DeviceInfo::FormFactor::kDesktop,
      /*signin_scoped_device_id=*/"signin_scoped_device_id",
      /*manufacturer_name=*/"manufacturer_name",
      /*model_name=*/"",
      /*full_hardware_class=*/"full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::TimeDelta(),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*sharing_info=*/absl::nullopt, std::move(paask_info),
      /*fcm_registration_token=*/"fcm_token", syncer::ModelTypeSet());
}

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
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnListSyncedPasskeys, syncer::kSyncWebauthnCredentials},
        /*disabled_features=*/{});
    ASSERT_TRUE(https_server_.InitializeAndListen());

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &WebAuthnAutofillIntegrationTest::RegisterTestServiceFactories,
                base::Unretained(this)));

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
    GURL url = https_server_.GetURL(kRpId, "/");
    signin_form.signon_realm = url.spec();
    signin_form.url = url;
    signin_form.action = url;
    signin_form.username_value = u"remilia";
    signin_form.password_value = u"shouldbeusingapasskeyinstead";
    base::RunLoop run_loop;
    password_store->AddLogin(signin_form, run_loop.QuitClosure());

    // Mock bluetooth support to allow discovery of fake hybrid devices.
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent)
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered)
        .WillByDefault(testing::Return(true));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);
    // Other parts of Chrome may keep a reference to the bluetooth adapter.
    // Since we do not verify any expectations, it is okay to leak this mock.
    testing::Mock::AllowLeak(mock_bluetooth_adapter_.get());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_server_.GetURL(kRpId, "/webauthn_conditional_mediation.html")));
  }

  void RegisterTestServiceFactories(content::BrowserContext* context) {
    PasskeyModelFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::FakeDeviceInfoSyncService>();
        }));
    // Disable the sync service by injecting a test fake. The sync service fails
    // to start when overriding the DeviceInfoSyncService with a test fake.
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
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
      if (suggestions[i].popup_item_id ==
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
      if (suggestions[suggestion_index].popup_item_id ==
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
      EXPECT_NE(suggestion.popup_item_id,
                autofill::PopupItemId::kWebauthnCredential);
      EXPECT_NE(suggestion.popup_item_id,
                autofill::PopupItemId::kWebauthnSignInWithAnotherDevice);
    }
  }

  virtual std::u16string GetDeviceString() = 0;

  scoped_refptr<device::MockBluetoothAdapter> mock_bluetooth_adapter_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  device::FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
        kCredentialID1, kRpId, std::vector<uint8_t>{5, 6, 7, 8}, "flandre",
        "Flandre Scarlet");
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
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
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
      kCredentialID2, kRpId, std::vector<uint8_t>{6, 7, 8, 9}, "sakuya",
      "Sakuya Izayoi");
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest, GPMPasskeys) {
  // Have the virtual device masquerade as a phone.
  virtual_device_factory_->SetTransport(device::FidoTransportProtocol::kHybrid);

  // Inject a fake phone from sync.
  syncer::DeviceInfo device_info = CreateDeviceInfo();
  auto* tracker = static_cast<syncer::FakeDeviceInfoTracker*>(
      DeviceInfoSyncServiceFactory::GetForProfile(browser()->profile())
          ->GetDeviceInfoTracker());
  tracker->Add(&device_info);

  // Inject a GPM passkey.
  PasskeyModelFactory::GetForProfile(browser()->profile())
      ->AddNewPasskeyForTesting(CreatePasskey());

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
  size_t suggestion_index = 0;
  size_t webauthn_entry_count = 0;
  autofill::Suggestion webauthn_entry;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (suggestions[i].popup_item_id ==
        autofill::PopupItemId::kWebauthnCredential) {
      webauthn_entry = suggestions[i];
      suggestion_index = i;
      webauthn_entry_count++;
    }
  }
  ASSERT_EQ(webauthn_entry_count, 1u);
  ASSERT_LT(suggestion_index, suggestions.size()) << "WebAuthn entry not found";
  EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
  EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value, kPhoneName);
  EXPECT_EQ(webauthn_entry.icon, "globeIcon");

  // Click the credential.
  popup_controller->AcceptSuggestionWithoutThreshold(suggestion_index);
  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");

  // The tracker outlives the test. Clean up the device_info to avoid flakiness.
  tracker->Remove(&device_info);
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
    device::PublicKeyCredentialRpEntity rp(kRpId);
    fake_webauthn_api_->InjectDiscoverableCredential(
        kCredentialID1, std::move(rp), std::move(user));

    win_webauthn_api_override_ =
        std::make_unique<device::WinWebAuthnApi::ScopedOverride>(
            fake_webauthn_api_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's, these objects need to be destroyed before
    // the test class.
    win_webauthn_api_override_.reset();
    WebAuthnAutofillIntegrationTest::PostRunTestOnMainThread();
  }

  std::u16string GetDeviceString() override {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_WINDOWS_HELLO);
  }

 protected:
  std::unique_ptr<device::FakeWinWebAuthnApi> fake_webauthn_api_;
  std::unique_ptr<device::WinWebAuthnApi::ScopedOverride>
      win_webauthn_api_override_;
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
  device::PublicKeyCredentialRpEntity rp(kRpId);
  fake_webauthn_api_->InjectDiscoverableCredential(
      kCredentialID2, std::move(rp), std::move(user));
  RunSelectAccountTest(kConditionalUIRequestFiltered);
}

IN_PROC_BROWSER_TEST_F(WebAuthnWindowsAutofillIntegrationTest, Abort) {
  RunAbortTest();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
