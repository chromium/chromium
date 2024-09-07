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
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/test/mock_trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
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
#include "device/fido/win/util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/util.h"
#endif  // BUILDFLAG(IS_MAC)

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
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt, std::move(paask_info),
      /*fcm_registration_token=*/"fcm_token", syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/base::Time::Now());
}

std::u16string ExpectedPasskeyLabel() {
  if (device::kWebAuthnGpmPin.Get()) {
    // In this case GPM should be enabled by default.
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_PASSKEY_FROM_GOOGLE_PASSWORD_MANAGER);
  } else {
    // Otherwise the label will mention the priority phone.
    return l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_PASSKEY_FROM_PHONE,
                                      kPhoneName);
  }
}

// Autofill integration tests. This file contains end-to-end tests for
// integration between WebAuthn and Autofill. These tests are sensitive to focus
// changes, so they are interactive UI tests.

// Base class for autofill integration tests, contains the actual test code but
// no setup.
class WebAuthnAutofillIntegrationTest : public CertVerifierBrowserTest {
 public:
  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(WebAuthnAutofillIntegrationTest* test_instance)
        : test_instance_(test_instance) {
      run_loop_ = std::make_unique<base::RunLoop>();
    }
    virtual ~DelegateObserver() = default;

    void WaitForUI() {
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      std::unique_ptr<
          testing::NiceMock<trusted_vault::MockTrustedVaultConnection>>
          connection = std::make_unique<
              testing::NiceMock<trusted_vault::MockTrustedVaultConnection>>();
      ON_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                               testing::_, testing::_))
          .WillByDefault(
              [](const CoreAccountInfo&,
                 base::OnceCallback<void(
                     trusted_vault::
                         DownloadAuthenticationFactorsRegistrationStateResult)>
                     callback) mutable {
                trusted_vault::
                    DownloadAuthenticationFactorsRegistrationStateResult result;
                result.state = trusted_vault::
                    DownloadAuthenticationFactorsRegistrationStateResult::
                        State::kEmpty;
                std::move(callback).Run(std::move(result));
                return std::make_unique<
                    trusted_vault::TrustedVaultConnection::Request>();
              });

      delegate->SetTrustedVaultConnectionForTesting(std::move(connection));
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      run_loop_->QuitWhenIdle();
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> ret;
      ret.emplace_back(TestPhone(base::UTF16ToUTF8(kPhoneName).c_str(),
                                 /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(1),
                                 /*channel_priority=*/1));
      return ret;
    }

   private:
    const raw_ptr<WebAuthnAutofillIntegrationTest> test_instance_;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

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
        {syncer::kSyncWebauthnCredentials},
        /*disabled_features=*/{
            // Disable this feature explicitly, as it can cause unexpected email
            // fields to be parsed in these tests.
            // TODO(crbug.com/1493145): Remove when/if launched.
            autofill::features::kAutofillEnableEmailHeuristicOnlyAddressForms});
    ASSERT_TRUE(https_server_.InitializeAndListen());

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &WebAuthnAutofillIntegrationTest::RegisterTestServiceFactories,
                base::Unretained(this)));

    CertVerifierBrowserTest::SetUp();

    // Log call `FIDO_LOG` messages.
    scoped_vmodule_.InitWithSwitches("device_event_log_impl=2");
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
        ProfilePasswordStoreFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS)
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

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_adaptor_->identity_test_env()->SetPrimaryAccount(
        "test@gmail.com", signin::ConsentLevel::kSync);

    delegate_observer_ = std::make_unique<DelegateObserver>(this);
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        delegate_observer_.get());

    mock_hw_provider_ =
        std::make_unique<crypto::ScopedMockUnexportableKeyProvider>();
    fake_uv_provider_ =
        std::make_unique<crypto::ScopedFakeUserVerifyingKeyProvider>();

#if BUILDFLAG(IS_MAC)
    biometrics_override_.reset();
    biometrics_override_ =
        std::make_unique<device::fido::mac::ScopedBiometricsOverride>(true);
#elif BUILDFLAG(IS_WIN)
    biometrics_override_.reset();
    biometrics_override_ =
        std::make_unique<device::fido::win::ScopedBiometricsOverride>(true);
#endif

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_server_.GetURL(kRpId, "/webauthn_conditional_mediation.html")));
  }

  void RegisterTestServiceFactories(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
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
    autofill_client->SetKeepPopupOpenForTesting(true);

    // Execute the Conditional UI request.
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, request);

    delegate_observer_->WaitForUI();

    // Interact with the username field until the popup shows up. This has the
    // effect of waiting for the browser to send the renderer the password
    // information, and waiting for the UI to render.
    base::WeakPtr<autofill::AutofillSuggestionController> suggestion_controller;
    while (!suggestion_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      suggestion_controller =
          autofill_client->suggestion_controller_for_testing();
    }

    // Find the webauthn credential on the suggestions list.
    auto suggestions = suggestion_controller->GetSuggestions();
    size_t suggestion_index = 0;
    size_t webauthn_entry_count = 0;
    autofill::Suggestion webauthn_entry;
    for (size_t i = 0; i < suggestions.size(); ++i) {
      if (suggestions[i].type ==
          autofill::SuggestionType::kWebauthnCredential) {
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
    EXPECT_EQ(webauthn_entry.icon, autofill::Suggestion::Icon::kGlobe);

    // Click the credential.
    test_api(static_cast<autofill::AutofillPopupControllerImpl&>(
                 *suggestion_controller))
        .DisableThreshold(true);
    suggestion_controller->AcceptSuggestion(suggestion_index);
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
    autofill_client->SetKeepPopupOpenForTesting(true);

    // Execute the Conditional UI request.
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);

    delegate_observer_->WaitForUI();

    // Interact with the username field until the popup shows up. This has the
    // effect of waiting for the browser to send the renderer the password
    // information, and waiting for the UI to render.
    base::WeakPtr<autofill::AutofillSuggestionController> suggestion_controller;
    while (!suggestion_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      suggestion_controller =
          autofill_client->suggestion_controller_for_testing();
    }

    // Find the webauthn credential on the suggestions list.
    auto suggestions = suggestion_controller->GetSuggestions();
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
    ASSERT_LT(suggestion_index, suggestions.size())
        << "WebAuthn entry not found";
    EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
    EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value, GetDeviceString());
    EXPECT_EQ(webauthn_entry.icon, autofill::Suggestion::Icon::kGlobe);

    // Abort the request.
    content::ExecuteScriptAsync(web_contents,
                                "window.requestAbortController.abort()");
    std::string result;
    ASSERT_TRUE(message_queue.WaitForMessage(&result));
    EXPECT_EQ(result, "\"error AbortError: signal is aborted without reason\"");

    // The popup may have gone away while waiting. If not, make sure it's gone.
    if (suggestion_controller) {
      suggestion_controller->Hide(
          autofill::SuggestionHidingReason::kUserAborted);
    }

    // Interact with the username field. Since there is still a saved password,
    // the popup should eventually show up.
    while (!suggestion_controller) {
      content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
      suggestion_controller =
          autofill_client->suggestion_controller_for_testing();
    }
    for (const auto& suggestion : suggestion_controller->GetSuggestions()) {
      EXPECT_NE(suggestion.type, autofill::SuggestionType::kWebauthnCredential);
      EXPECT_NE(suggestion.type,
                autofill::SuggestionType::kWebauthnSignInWithAnotherDevice);
    }
  }

  virtual std::u16string GetDeviceString() = 0;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  scoped_refptr<device::MockBluetoothAdapter> mock_bluetooth_adapter_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  device::FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_;
  std::unique_ptr<DelegateObserver> delegate_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  logging::ScopedVmoduleSwitches scoped_vmodule_;
  std::unique_ptr<crypto::ScopedMockUnexportableKeyProvider> mock_hw_provider_;
  std::unique_ptr<crypto::ScopedFakeUserVerifyingKeyProvider> fake_uv_provider_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<device::fido::win::ScopedBiometricsOverride>
      biometrics_override_;
#elif BUILDFLAG(IS_MAC)
  std::unique_ptr<device::fido::mac::ScopedBiometricsOverride>
      biometrics_override_;
#endif
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

IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest,
                       GPMPasskeys) {
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
  autofill_client->SetKeepPopupOpenForTesting(true);

  // Execute the Conditional UI request.
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);

  delegate_observer_->WaitForUI();

  // Interact with the username field until the popup shows up. This has the
  // effect of waiting for the browser to send the renderer the password
  // information, and waiting for the UI to render.
  base::WeakPtr<autofill::AutofillSuggestionController> suggestion_controller;
  while (!suggestion_controller) {
    content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
    suggestion_controller =
        autofill_client->suggestion_controller_for_testing();
  }

  // Find the webauthn credential on the suggestions list.
  auto suggestions = suggestion_controller->GetSuggestions();
  size_t suggestion_index = 0;
  size_t webauthn_entry_count = 0;
  autofill::Suggestion webauthn_entry;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (suggestions[i].type == autofill::SuggestionType::kWebauthnCredential) {
      webauthn_entry = suggestions[i];
      suggestion_index = i;
      webauthn_entry_count++;
    }
  }
  ASSERT_EQ(webauthn_entry_count, 1u);
  ASSERT_LT(suggestion_index, suggestions.size()) << "WebAuthn entry not found";
  EXPECT_EQ(webauthn_entry.main_text.value, u"flandre");
  EXPECT_EQ(webauthn_entry.labels.at(0).at(0).value, ExpectedPasskeyLabel());
  EXPECT_EQ(webauthn_entry.icon, autofill::Suggestion::Icon::kGlobe);

  // Click the credential.
  test_api(static_cast<autofill::AutofillPopupControllerImpl&>(
               *suggestion_controller))
      .DisableThreshold(true);
  suggestion_controller->AcceptSuggestion(suggestion_index);
  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");

  // Tapping a GPM passkey will not automatically hide the popup
  // because the enclave might still be loading. Manually hide the
  // popup so that the autofill client can be destroyed, avoiding
  // a DCHECK on test tear down.
  autofill_client->HideAutofillSuggestions(
      autofill::SuggestionHidingReason::kTabGone);
  // The tracker outlives the test. Clean up the device_info to avoid flakiness.
  tracker->Remove(&device_info);
}

// Tests that downloading passkeys from sync during a conditional UI also
// updates the autofill popup with the newly downloaded credentials.
IN_PROC_BROWSER_TEST_F(WebAuthnDevtoolsAutofillIntegrationTest,
                       GPMPasskeys_UpdatePasskeys) {
  // Have the virtual device masquerade as a phone.
  virtual_device_factory_->SetTransport(device::FidoTransportProtocol::kHybrid);

  // Make sure input events cannot close the autofill popup.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  autofill::ChromeAutofillClient* autofill_client =
      autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents);
  autofill_client->SetKeepPopupOpenForTesting(true);

  // Execute the Conditional UI request.
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);

  delegate_observer_->WaitForUI();

  // Interact with the username field until the popup shows up. This has the
  // effect of waiting for the browser to send the renderer the password
  // information, and waiting for the UI to render.
  base::WeakPtr<autofill::AutofillSuggestionController> suggestion_controller;
  while (!suggestion_controller) {
    content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
    suggestion_controller =
        autofill_client->suggestion_controller_for_testing();
  }

  // There should be no webauthn suggestions.
  auto suggestions = suggestion_controller->GetSuggestions();
  for (const auto& suggestion : suggestions) {
    ASSERT_NE(suggestion.type, autofill::SuggestionType::kWebauthnCredential);
  }

  // Simulate the user opting in to sync by injecting a phone and a passkey.
  syncer::DeviceInfo device_info = CreateDeviceInfo();
  auto* tracker = static_cast<syncer::FakeDeviceInfoTracker*>(
      DeviceInfoSyncServiceFactory::GetForProfile(browser()->profile())
          ->GetDeviceInfoTracker());
  tracker->Add(&device_info);

  // Inject a GPM passkey.
  PasskeyModelFactory::GetForProfile(browser()->profile())
      ->AddNewPasskeyForTesting(CreatePasskey());

  // The newly added passkey should be added to the popup. The request needs
  // time to restart, poll the popup until the new entry shows up.
  std::optional<autofill::Suggestion> webauthn_entry;
  size_t suggestion_index;
  while (!webauthn_entry) {
    content::SimulateMouseClickOrTapElementWithId(web_contents, "username");
    suggestion_controller =
        autofill_client->suggestion_controller_for_testing();
    suggestions = suggestion_controller->GetSuggestions();
    for (size_t i = 0; i < suggestions.size(); ++i) {
      if (suggestions[i].type ==
          autofill::SuggestionType::kWebauthnCredential) {
        webauthn_entry = suggestions[i];
        suggestion_index = i;
      }
    }
  }
  EXPECT_EQ(webauthn_entry->main_text.value, u"flandre");
  EXPECT_EQ(webauthn_entry->labels.at(0).at(0).value, ExpectedPasskeyLabel());
  EXPECT_EQ(webauthn_entry->icon, autofill::Suggestion::Icon::kGlobe);

  // Click the credential.
  test_api(static_cast<autofill::AutofillPopupControllerImpl&>(
               *suggestion_controller))
      .DisableThreshold(true);
  suggestion_controller->AcceptSuggestion(suggestion_index);
  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");

  // Tapping a GPM passkey will not automatically hide the popup
  // because the enclave might still be loading. Manually hide the
  // popup so that the autofill client can be destroyed, avoiding
  // a DCHECK on test tear down.
  autofill_client->HideAutofillSuggestions(
      autofill::SuggestionHidingReason::kTabGone);
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
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_PASSKEY_FROM_WINDOWS_HELLO);
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
