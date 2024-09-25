// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <sstream>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

static constexpr uint8_t kCredentialID[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                            9, 10, 11, 12, 13, 14, 15, 16};
static constexpr uint8_t kCredentialID2[] = {16, 15, 14, 13, 12, 11, 10, 9,
                                             8,  7,  6,  5,  4,  3,  2,  1};
constexpr uint8_t kUserId1[] = {1, 2, 3, 4};
constexpr uint8_t kUserId2[] = {5, 6, 7, 8};
constexpr char kUsername1[] = "flandre";
constexpr char kDisplayName1[] = "Flandre Scarlet";
constexpr char kUsername2[] = "sakuya";
constexpr char kDisplayName2[] = "Sakuya Izayoi";

sync_pb::WebauthnCredentialSpecifics CreateWebAuthnCredentialSpecifics(
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> user_id,
    const char* username,
    const char* display_name) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(credential_id.data(), credential_id.size());
  passkey.set_rp_id("www.example.com");
  passkey.set_user_id(user_id.data(), user_id.size());
  passkey.set_user_name(username);
  passkey.set_user_display_name(display_name);
  return passkey;
}

// This file tests WebAuthn features that depend on specific //chrome behaviour.
// Tests that don't depend on that should go into
// content/browser/webauth/webauth_browsertest.cc.
class WebAuthnBrowserTest : public CertVerifierBrowserTest {
 public:
  WebAuthnBrowserTest() = default;
  WebAuthnBrowserTest(const WebAuthnBrowserTest&) = delete;
  WebAuthnBrowserTest& operator=(const WebAuthnBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
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
    mock_cert_verifier()->set_default_result(net::OK);

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
  }

 protected:
  scoped_refptr<device::MockBluetoothAdapter> mock_bluetooth_adapter_ = nullptr;
  device::FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  const base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnRelatedOrigin};
};

static constexpr char kGetAssertionCredID1234[] = R"((() => {
  let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
  return navigator.credentials.get({ publicKey: {
    challenge: cred_id,
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [{type: 'public-key', id: cred_id}],
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

static constexpr char kMakeCredential[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

static constexpr char kMakeDiscoverableCredential[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    authenticatorSelection: {
      requireResidentKey: true,
    },
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ChromeExtensions) {
  // Test that WebAuthn works inside of Chrome extensions. WebAuthn is based on
  // Relying Party IDs, which are domain names. But Chrome extensions don't have
  // domain names therefore the origin is used in their case.
  //
  // This test creates and installs an extension and then loads an HTML page
  // from inside that extension. A WebAuthn call is injected into that context
  // and it should get an assertion from a credential that's injected into the
  // virtual authenticator, scoped to the origin string.
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";

  static constexpr char kContents[] = R"(
<html>
  <head>
    <title>WebAuthn in extensions test</title>
  </head>
  <body>
  </body>
</html>
)";
  WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents);

  static constexpr char kExtensionSite[] = "https://extension-site.com/";
  static constexpr char kWebAccessibleResources[] =
      R"([{
            "resources": ["page.html"],
            "matches": ["*://*/*"]
         }])";

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .AddHostPermission(kExtensionSite)
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources",
                      base::test::ParseJson(kWebAccessibleResources));

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();
  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  const GURL url = extension->GetResourceURL(kPageFile);
  auto extension_id = url.host();
  virtual_device_factory->mutable_state()->InjectRegistration(
      kCredentialID, "chrome-extension://" + extension_id);

  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::move(virtual_device_factory));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionCredID1234));

  static constexpr char kMakeCredentialCrossDomainWithHostPerms[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { id: "extension-site.com", name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

  // This should work as the extension has host permissions over the site.
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialCrossDomainWithHostPerms)
          .ExtractString(),
      "webauthn: OK");

  static constexpr char kMakeCredentialCrossDomainNoHostPerms[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { id: "example.com", name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

  // This should fail with INVALID_PROTOCOL and never one of the errors from
  // related-origin processing because extensions don't participate in that
  // system.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialCrossDomainNoHostPerms)
          .ExtractString(),
      testing::HasSubstr("Public-key credentials are only available to"));
}

#if BUILDFLAG(IS_WIN)
// Integration test for Large Blob on Windows.
IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, WinLargeBlob) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  device::FakeWinWebAuthnApi fake_api;
  fake_api.set_version(WEBAUTHN_API_VERSION_3);
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&fake_api);

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  virtual_device_factory->set_discover_win_webauthn_api_authenticator(true);
  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::move(virtual_device_factory));

  constexpr char kMakeCredentialLargeBlob[] = R"(
    let cred_id;
    const blob = "blobby volley";
    navigator.credentials.create({ publicKey: {
      challenge: new TextEncoder().encode('climb a mountain'),
      rp: { name: 'Acme' },
      user: {
        id: new TextEncoder().encode('1098237235409872'),
        name: 'avery.a.jones@example.com',
        displayName: 'Avery A. Jones'},
      pubKeyCredParams: [{ type: 'public-key', alg: '-257'}],
      authenticatorSelection: {
         requireResidentKey: true,
      },
      extensions: { largeBlob: { support: 'required' } },
    }}).then(cred => {
      cred_id = cred.rawId;
      if (!cred.getClientExtensionResults().largeBlob ||
          !cred.getClientExtensionResults().largeBlob.supported) {
        throw new Error('large blob not supported');
      }
      return navigator.credentials.get({ publicKey: {
        challenge: new TextEncoder().encode('run a marathon'),
        allowCredentials: [{type: 'public-key', id: cred_id}],
        extensions: {
          largeBlob: {
            write: new TextEncoder().encode(blob),
          },
        },
      }});
    }).then(assertion => {
      if (!assertion.getClientExtensionResults().largeBlob.written) {
        throw new Error('large blob not written to');
      }
      return navigator.credentials.get({ publicKey: {
        challenge: new TextEncoder().encode('solve p=np'),
        allowCredentials: [{type: 'public-key', id: cred_id}],
        extensions: {
          largeBlob: {
            read: true,
          },
        },
      }});
    }).then(assertion => {
      if (new TextDecoder().decode(
          assertion.getClientExtensionResults().largeBlob.blob) != blob) {
        throw new Error('blob does not match');
      }
      return 'webauthn: OK';
    }).catch(error => 'webauthn: ' + error.toString());)";

  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialLargeBlob));
}
#endif  // BUILDFLAG(IS_WIN)

class WebAuthnGpmPasskeyTest : public WebAuthnBrowserTest {
 public:
  class Observer : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    virtual ~Observer() = default;

    std::optional<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
    transport_availability_info() {
      return transport_availability_info_;
    }

    void WaitForUI() { run_loop_.Run(); }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {}

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> ret;
      ret.emplace_back(TestPhone("phone", /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(1),
                                 /*channel_priority=*/1));
      return ret;
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {
      transport_availability_info_ = *tai;
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      run_loop_.Quit();
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {}

   private:
    base::RunLoop run_loop_;
    std::optional<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
        transport_availability_info_;
  };

  WebAuthnGpmPasskeyTest() {
    scoped_feature_list_.InitWithFeatures({syncer::kSyncWebauthnCredentials},
                                          /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<Observer>();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        observer_.get());
  }

  void PostRunTestOnMainThread() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebAuthnBrowserTest::SetUpInProcessBrowserTestFixture();
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &WebAuthnGpmPasskeyTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    PasskeyModelFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
  }

  std::unique_ptr<Observer> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription subscription_;
};

// Tests that chrome filters out GPM passkeys that don't appear on a request
// allow list.
IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest, FilterGPMPasskeys) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // Set up two GPM passkeys.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID2, kUserId2, kUsername2, kDisplayName2));

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  virtual_device_factory->SetTransport(device::FidoTransportProtocol::kHybrid);
  virtual_device_factory->mutable_state()->InjectResidentKey(
      kCredentialID, "www.example.com", kUserId1, kUsername1, kDisplayName1);
  virtual_device_factory->mutable_state()->InjectResidentKey(
      kCredentialID2, "www.example.com", kUserId2, kUsername2, kDisplayName2);
  virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  virtual_device_factory->SetCtap2Config(std::move(config));
  auto auth_env =
      std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
          std::move(virtual_device_factory));

  // Request an assertion with a credential ID matching only the first passkey.
  content::ExecuteScriptAsync(
      browser()->tab_strip_model()->GetActiveWebContents(),
      kGetAssertionCredID1234);

  observer_->WaitForUI();

  // Only the first passkey should be in the recognized credentials list.
  device::DiscoverableCredentialMetadata expected(
      device::AuthenticatorType::kPhone, "www.example.com",
      device::fido_parsing_utils::Materialize(kCredentialID),
      device::PublicKeyCredentialUserEntity(
          device::fido_parsing_utils::Materialize(kUserId1), kUsername1,
          kDisplayName1));
  EXPECT_THAT(observer_->transport_availability_info()->recognized_credentials,
              testing::ElementsAre(expected));
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest,
                       SignalUnknownCredentialGPMPasskeys) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));

  // Reports the credential ID matching the passkey created.
  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     R"(
  PublicKeyCredential.signalUnknownCredential({
    rpId: "www.example.com",
    credentialId: "AQIDBAUGBwgJCgsMDQ4PEA",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )"));

  // After reporting the passkey, it should be deleted from the credentials
  // list, so the vector of passkeys matching the relying party should be empty.
  EXPECT_TRUE(
      passkey_model->GetPasskeysForRelyingPartyId("www.example.com").empty());
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest,
                       SignalAllAcceptedCredsNoPasskeyDeletion) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));

  // Reports the user ID and credential ID matching the created passkey.
  // The passkey will not be deleted.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    userId: "AQIDBA",
    allAcceptedCredentialIds: ["AQIDBAUGBwgJCgsMDQ4PEA"],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )"));

  // Check that the passkey with kCredentialID was not deleted.
  EXPECT_TRUE(passkey_model->GetPasskeyByCredentialId(
      "www.example.com",
      std::string(reinterpret_cast<const char*>(kCredentialID), 16)));

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // If the model_state is INACTIVE_STATE, it means that DeletePasskey didn't
  // run, and hence the Passkey Not Accepted bubble did not show up.
  EXPECT_EQ(model_state, password_manager::ui::INACTIVE_STATE);
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest,
                       SignalAllAcceptedCredsPasskeyDeletion) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID2, kUserId2, kUsername2, kDisplayName2));

  // Reports the user ID that matches the passkey created with an empty
  // allCurrentCredentialIds. The passkey will be deleted.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    userId: "BQYHCA",
    allAcceptedCredentialIds: [],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )"));

  // Check that the passkey with kCredentialID2 was deleted.
  EXPECT_FALSE(passkey_model->GetPasskeyByCredentialId(
      "www.example.com",
      std::string(reinterpret_cast<const char*>(kCredentialID2), 16)));

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // Check if the Passkey Not Accepted bubble showed up.
  EXPECT_EQ(model_state, password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE);
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest, ReportInvalidStrings) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // This should fail with a TypeError due to an invalid base64url
  // string in the userId.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
      rpId: "www.example.com",
      userId: "a/+c+/c",
      allAcceptedCredentialIds: ["AQIDBAUGBwgJCgsMDQ4PEA"],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for userId."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the allAcceptedCredentialIds list.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    userId: "BQYHCA",
    allAcceptedCredentialIds: ["/a+/b+/c"],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr(
          "Invalid base64url string for allAcceptedCredentialIds."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the userId of signalCurrentUserDetails;
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalCurrentUserDetails({
    rpId: "www.example.com",
    userId: "A+/+A",
    name: "Pepito",
    displayName: "Pepito The Cat",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for userId."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the credentialId report.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalUnknownCredential({
    rpId: "www.example.com",
    credentialId: "A+/+A",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for credentialId."));
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest,
                       SignalCurrentUserDetailsGPMPasskeys) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));

  // Reports the user ID that matches the passkey created with the
  // current user details.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalCurrentUserDetails({
    rpId: "www.example.com",
    userId: "AQIDBA",
    name: "Pepito",
    displayName: "Pepito The Cat",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )"));

  auto passkey = passkey_model->GetPasskeyByCredentialId(
      "www.example.com",
      std::string(reinterpret_cast<const char*>(kCredentialID), 16));

  // Check if the name and displayName of the passkey reported was updated.
  EXPECT_EQ(passkey->user_name(), "Pepito");
  EXPECT_EQ(passkey->user_display_name(), "Pepito The Cat");

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // Check if the Passkey Updated bubble showed up.
  EXPECT_EQ(model_state,
            password_manager::ui::PASSKEY_UPDATED_CONFIRMATION_STATE);
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest, SignalCurrentUserDetailsQuota) {
  constexpr char kRequest[] = R"(
    PublicKeyCredential.signalCurrentUserDetails({
      rpId: "www.example.com",
      userId: "AQIDBA",
      name: "$1",
      displayName: "Pepito The Cat",
    }).then(c => 'webauthn: OK', e => 'error ' + e);
    )";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));

  // Call the signal methods enough that we run into the quota.
  for (int i = 0; i < webauthn::PasskeyChangeQuotaTracker::kMaxTokensPerRP;
       ++i) {
    EXPECT_EQ(
        "webauthn: OK",
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        base::ReplaceStringPlaceholders(
                            kRequest, {base::NumberToString(i)}, nullptr)));
  }

  // This request should be silently dropped now.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(
          browser()->tab_strip_model()->GetActiveWebContents(),
          base::ReplaceStringPlaceholders(kRequest, {kUsername1}, nullptr)));

  // Check that the name hasn't been updated.
  auto passkey = passkey_model->GetPasskeyByCredentialId(
      "www.example.com",
      std::string(reinterpret_cast<const char*>(kCredentialID), 16));
  EXPECT_NE(passkey->user_name(), kUsername1);
}

IN_PROC_BROWSER_TEST_F(WebAuthnGpmPasskeyTest,
                       SignalCurrentUserDetailsWithNoChanges) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID, kUserId1, kUsername1, kDisplayName1));

  // Reports the user ID that matches the passkey created with the
  // current user details.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalCurrentUserDetails({
    rpId: "www.example.com",
    userId: "AQIDBA",
    name: "flandre",
    displayName: "Flandre Scarlet",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )"));

  auto passkey = passkey_model->GetPasskeyByCredentialId(
      "www.example.com",
      std::string(reinterpret_cast<const char*>(kCredentialID), 16));

  // Check if the name and displayName of the passkey reported did not change.
  EXPECT_EQ(passkey->user_name(), kUsername1);
  EXPECT_EQ(passkey->user_display_name(), kDisplayName1);

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // If the model_state is INACTIVE_STATE, it means that UpdatePasskey didn't
  // run, and hence the Passkey Updated bubble did not show up.
  EXPECT_EQ(model_state, password_manager::ui::INACTIVE_STATE);
}

class WebAuthnHintsTest : public WebAuthnBrowserTest {
  class Observer : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    virtual ~Observer() = default;

    void WaitForHints() { run_loop_.Run(); }

    const content::AuthenticatorRequestClientDelegate::Hints& hints() const {
      return hints_;
    }

    void HintsSet(const content::AuthenticatorRequestClientDelegate::Hints&
                      hints) override {
      hints_ = hints;
      run_loop_.Quit();
    }

   private:
    content::AuthenticatorRequestClientDelegate::Hints hints_;
    base::RunLoop run_loop_;
  };

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<Observer>();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        observer_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

// The hints parameter here contains nonsense values (which should be ignored)
// and lists `security-key` and `hybrid` (more than once). This is contradictory
// but Chromium will prioritize in the order of the enum values, so
// `security-key` will win out.
static constexpr char kMakeCredentialWithHints[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    hints: ["nonsense", "hybrid", "security-key", "hybrid", "nonsense"],
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

IN_PROC_BROWSER_TEST_F(WebAuthnHintsTest, HintsArePassedThrough) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialWithHints);
  observer_->WaitForHints();

  ASSERT_TRUE(observer_->hints().transport.has_value());
  EXPECT_EQ(observer_->hints().transport.value(),
            AuthenticatorTransport::kUsbHumanInterfaceDevice);
}

class WebAuthnConditionalUITest : public WebAuthnBrowserTest {
  class Observer : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    enum State {
      kHasNotShowedUI,
      kWaitingForUI,
      kShowedUI,
    };
    virtual ~Observer() = default;
    void WaitForUI() {
      if (state_ != kHasNotShowedUI) {
        return;
      }
      state_ = kWaitingForUI;
      run_loop_.Run();
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      delegate_ = delegate;
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      return {};
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {}

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      if (state_ == kWaitingForUI) {
        // When the content layer controls authenticator dispatch, dispatching
        // happens on tasks posted right before the UI is shown. We need to
        // QuitWhenIdle to make sure that, if an authenticator is dispatched to,
        // that task has a chance to finish before the test continues. That way
        // we can catch any potentially unexpected authenticator dispatches.
        run_loop_.QuitWhenIdle();
      }
      state_ = kShowedUI;
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {
      for (const auto& response : responses) {
        accounts_.emplace_back(base::HexEncode(response.credential->id));
      }
    }

    raw_ptr<ChromeAuthenticatorRequestDelegate, AcrossTasksDanglingUntriaged>
        delegate_ = nullptr;
    std::vector<std::string> accounts_;

   private:
    State state_ = kHasNotShowedUI;
    base::RunLoop run_loop_;
  };

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<Observer>();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    virtual_device_factory->mutable_state()->InjectResidentKey(
        kCredentialID, "www.example.com", std::vector<uint8_t>{5, 6, 7, 8},
        "flandre", "Flandre Scarlet");
    virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    config.internal_uv_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        observer_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

static constexpr char kConditionalUIRequest[] = R"((() => {
window.requestAbortController = new AbortController();
navigator.credentials.get({
  signal: window.requestAbortController.signal,
  mediation: 'conditional',
  publicKey: {
    challenge: new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]),
    timeout: 10000,
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

// Tests that the "Sign in with another device…" button dispatches requests to
// plugged in authenticators.
IN_PROC_BROWSER_TEST_F(WebAuthnConditionalUITest,
                       ConditionalUIOtherDeviceButton) {
  // Make a Conditional UI request. The authenticator should not be dispatched
  // to before the user clicks the "Sign in with another device…" button.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([](device::VirtualFidoDevice* device) {
        CHECK(false) << "Virtual device should not have been dispatched to";
        return false;
      });
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);
  observer_->WaitForUI();

  // Allow the virtual device to respond to requests, then simulate clicking the
  // "Sign in with another device…" button and wait for a result.
  base::RunLoop run_loop;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return true; });
  ChromeWebAuthnCredentialsDelegate delegate(web_contents);
  delegate.LaunchSecurityKeyOrHybridFlow();

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");
  EXPECT_EQ(observer_->accounts_.size(), 1u);
  EXPECT_EQ(observer_->accounts_.at(0), "0102030405060708090A0B0C0D0E0F10");
}

// WebAuthnCableExtension exercises code paths where a server sends a caBLEv2
// extension in a get() request.
class WebAuthnCableExtension : public WebAuthnBrowserTest {
 public:
  WebAuthnCableExtension() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthCableExtensionAnywhere}, {});
  }

  void PostRunTestOnMainThread() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  static constexpr char kRequest[] = R"((() => {
    return navigator.credentials.get({
      publicKey: {
        timeout: 1000,
        challenge: new Uint8Array([
            0x79, 0x50, 0x68, 0x71, 0xDA, 0xEE, 0xEE, 0xB9,
            0x94, 0xC3, 0xC2, 0x15, 0x67, 0x65, 0x26, 0x22,
            0xE3, 0xF3, 0xAB, 0x3B, 0x78, 0x2E, 0xD5, 0x6F,
            0x81, 0x26, 0xE2, 0xA6, 0x01, 0x7D, 0x74, 0x50
        ]).buffer,
        allowCredentials: [{
          type: 'public-key',
          id: new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]).buffer,
        }],
        userVerification: 'discouraged',

        extensions: {
          "cableAuthentication": [{
            version: 2,
            sessionPreKey: new Uint8Array([$1]).buffer,
            clientEid: new Uint8Array(),
            authenticatorEid: new Uint8Array(),
          }],
        },
      },
    }).then(c => 'webauthn: OK',
            e => 'error ' + e);
  })())";

  void DoRequest(std::string server_link_data) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory->mutable_state()->InjectRegistration(
        kCredentialID, "www.example.com");
    std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting>
        auth_env =
            std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
                std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);

    const std::string request =
        base::ReplaceStringPlaceholders(kRequest, {server_link_data}, nullptr);

    EXPECT_EQ(
        "webauthn: OK",
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        request));
  }

  class ExtensionObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {}

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      return {};
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {}

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {}

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {
      extensions_.emplace_back(base::HexEncode(server_link_data));
    }

    std::vector<std::string> extensions_;
  };

  ExtensionObserver observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnCableExtension, ServerLink) {
  DoRequest("1,2,3,4");

  ASSERT_EQ(observer_.extensions_.size(), 1u);
  EXPECT_EQ(observer_.extensions_[0], "01020304");
}

// WebAuthnCableSecondFactor primarily exercises
// ChromeAuthenticatorRequestDelegate and AuthenticatorRequestDialogController.
// It mocks out the discovery process and thus allows the caBLE UI to be tested.
// It uses a trace-based approach: events are recorded (as strings) in an event
// trace which is then compared against the expected trace at the end.
class WebAuthnCableSecondFactor : public WebAuthnBrowserTest {
 public:
  WebAuthnCableSecondFactor() {
    // This makes it a little easier to compare against.
    trace_ << std::endl;
  }

  std::ostringstream& trace() { return trace_; }

  raw_ptr<AuthenticatorRequestDialogController, DanglingUntriaged>&
  controller() {
    return controller_;
  }

 protected:
  // DiscoveryFactory vends a single discovery that doesn't discover anything
  // until requested to. The authenticator that is then discovered is a virtual
  // authenticator that serves simply to end the overall WebAuthn request.
  // Otherwise, DiscoveryFactory is responsible for tracing the caBLEv2 Pairing
  // objects and driving the simulation when the UI requests that a phone be
  // triggered.
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(WebAuthnCableSecondFactor* test)
        : parent_(test) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport != device::FidoTransportProtocol::kHybrid) {
        return {};
      }

      auto discovery = std::make_unique<PendingDiscovery>(
          device::FidoTransportProtocol::kHybrid);
      add_authenticator_callback_ = discovery->GetAddAuthenticatorCallback();
      return SingleDiscovery(std::move(discovery));
    }

    void set_cable_data(
        device::FidoRequestType request_type,
        std::vector<device::CableDiscoveryData> cable_data,
        const std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
            qr_generator_key) override {
      parent_->trace() << "SET_CABLE_DATA" << std::endl;
    }

    void set_cable_invalidated_pairing_callback(
        base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
            callback) override {
      invalid_pairing_callback_ = std::move(callback);
    }

    base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
    get_cable_contact_callback() override {
      return base::BindLambdaForTesting(
          [this](std::unique_ptr<device::cablev2::Pairing> pairing) {
            parent_->trace()
                << "CONTACT: phone_name=" << pairing->name << " public_key="
                << static_cast<int>(pairing->peer_public_key_x962[0])
                << " step=" << contact_step_number_ << std::endl;
            switch (contact_step_number_) {
              case 0:
                // Simiulate the first tunnel failing with a Gone status. This
                // should trigger a fallback to the second-priority phone with
                // the same name.
                base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindOnce(invalid_pairing_callback_,
                                              std::move(pairing)));
                break;

              case 1:
                // Simulate the user clicking back and trying the phone again.
                // This should fallback to the lower-priority phone with the
                // same name.
                base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindLambdaForTesting([this]() {
                      parent_->controller()->ContactPhoneForTesting("name2");
                    }));
                break;

              case 2:
                // Try some other phones.
                base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindLambdaForTesting([this]() {
                      parent_->controller()->ContactPhoneForTesting("zzz");
                    }));
                break;

              case 3:
                // Try some other phones.
                base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindLambdaForTesting([this]() {
                      parent_->controller()->ContactPhoneForTesting("aaa");
                    }));
                break;

              case 4:
                // All done. Discover a virtual authenticator in order to
                // resolve the request.
                add_authenticator_callback_.Run();
                break;

              default:
                CHECK(false);
            }

            contact_step_number_++;
          });
    }

#if BUILDFLAG(IS_WIN)
    std::unique_ptr<device::FidoDiscoveryBase>
    MaybeCreateWinWebAuthnApiDiscovery() override {
      return nullptr;
    }
#endif  // BUILDFLAG(IS_WIN)

   private:
    // PendingDiscovery yields a single virtual authenticator when requested to
    // do so by calling the result of |GetAddAuthenticatorCallback|.
    class PendingDiscovery final : public device::FidoDeviceDiscovery {
     public:
      explicit PendingDiscovery(device::FidoTransportProtocol transport)
          : FidoDeviceDiscovery(transport) {}

      base::RepeatingClosure GetAddAuthenticatorCallback() {
        return base::BindRepeating(&PendingDiscovery::AddAuthenticator,
                                   weak_ptr_factory_.GetWeakPtr());
      }

     protected:
      void StartInternal() override {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&PendingDiscovery::NotifyDiscoveryStarted,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      /*success=*/true));
      }

     private:
      void AddAuthenticator() {
        scoped_refptr<device::VirtualFidoDevice::State> state(
            new device::VirtualFidoDevice::State);
        state->InjectRegistration(kCredentialID, "www.example.com");
        state->fingerprints_enrolled = true;

        device::VirtualCtap2Device::Config config;
        config.resident_key_support = true;
        config.internal_uv_support = true;

        AddDevice(std::make_unique<device::VirtualCtap2Device>(state, config));
      }

      base::WeakPtrFactory<PendingDiscovery> weak_ptr_factory_{this};
    };

    const raw_ptr<WebAuthnCableSecondFactor> parent_;
    base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
        invalid_pairing_callback_;
    base::RepeatingClosure add_authenticator_callback_;
    int contact_step_number_ = 0;
  };

  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(WebAuthnCableSecondFactor* test)
        : parent_(test) {}

    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      // Only a single delegate should be observed.
      CHECK(!parent_->controller());
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> ret;

      ret.emplace_back(TestPhone("name1", /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(1),
                                 /*channel_priority=*/1));

      // The same public key as phone1, but a newer timestamp. It
      // should shadow the first.
      ret.emplace_back(TestPhone("name2", /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(2),
                                 /*channel_priority=*/1));

      // Same name as the second, but a higher channel priority. It should take
      // priority over it.
      ret.emplace_back(TestPhone("name2", /*public_key=*/1,
                                 /*last_updated=*/base::Time::FromTimeT(2),
                                 /*channel_priority=*/2));

      // Same name as second and third, but a newer timestamp than the third. It
      // should be tried first.
      ret.emplace_back(TestPhone("name2", /*public_key=*/2,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      // A different device with a name that should sort first.
      ret.emplace_back(TestPhone("aaa", /*public_key=*/3,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      // A different device with a name that should sort last.
      ret.emplace_back(TestPhone("zzz", /*public_key=*/4,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      return ret;
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {
      tai->available_transports.insert(device::FidoTransportProtocol::kHybrid);
      tai->ble_status = device::FidoRequestHandlerBase::BleStatus::kOn;
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      parent_->controller() = delegate->dialog_controller();

      for (const auto& name :
           parent_->controller()->model()->paired_phone_names) {
        parent_->trace() << "UINAME: " << name << std::endl;
      }

      // Simulate a click on the transport selection sheet.
      parent_->controller()->ContactPhoneForTesting("name2");
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void ConfiguringCable(device::FidoRequestType request_type) override {
      switch (request_type) {
        case device::FidoRequestType::kMakeCredential:
          parent_->trace() << "TYPE: mc" << std::endl;
          break;
        case device::FidoRequestType::kGetAssertion:
          parent_->trace() << "TYPE: ga" << std::endl;
          break;
      }
    }

   private:
    const raw_ptr<WebAuthnCableSecondFactor> parent_;
  };

 protected:
  std::ostringstream trace_;
  raw_ptr<AuthenticatorRequestDialogController, DanglingUntriaged> controller_ =
      nullptr;
#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api_;
  device::WinWebAuthnApi::ScopedOverride override_win_webauthn_api_{
      &fake_win_webauthn_api_};
#endif
};

// TODO(crbug.com/40186172): this test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(WebAuthnCableSecondFactor, MAYBE_Test) {
  DelegateObserver observer(this);
  ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer);
  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::make_unique<DiscoveryFactory>(this));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionCredID1234));

  constexpr char kExpectedTrace[] = R"(
TYPE: ga
SET_CABLE_DATA
UINAME: aaa
UINAME: name2
UINAME: zzz
CONTACT: phone_name=name2 public_key=2 step=0
CONTACT: phone_name=name2 public_key=1 step=1
CONTACT: phone_name=name2 public_key=0 step=2
CONTACT: phone_name=zzz public_key=4 step=3
CONTACT: phone_name=aaa public_key=3 step=4
)";
  EXPECT_EQ(kExpectedTrace, trace_.str());
}

// These two tests are separate, rather than a for loop, because the testing
// infrastructure needs to be reset for each test and having a separate test
// is the easiest way to do that.

IN_PROC_BROWSER_TEST_F(WebAuthnCableSecondFactor, RequestTypesMakeCredential) {
  // Check that the correct request types are plumbed through.
  DelegateObserver observer(this);
  ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer);
  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::make_unique<DiscoveryFactory>(this));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredential));
  EXPECT_TRUE(trace_.str().find("TYPE: mc\n") != std::string::npos)
      << trace_.str();
}

IN_PROC_BROWSER_TEST_F(WebAuthnCableSecondFactor,
                       RequestTypesMakeDiscoverableCredential) {
  // Check that the correct request types are plumbed through.
  DelegateObserver observer(this);
  ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer);
  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::make_unique<DiscoveryFactory>(this));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeDiscoverableCredential));
  EXPECT_TRUE(trace_.str().find("TYPE: mc\n") != std::string::npos)
      << trace_.str();
}

}  // namespace
