// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <vector>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/mocks/mock_event_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_ui.h"
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
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/compiler_specific.h"
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
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

static constexpr char kSignalUnknownCredentialId[] = R"(
PublicKeyCredential.signalUnknownCredential({
  rpId: "www.example.com",
  credentialId: "$1",
}).then(c => 'webauthn: OK', e => 'error ' + e);
)";

// The hints parameter here contains nonsense values (which should be ignored)
// and lists `security-key` and `hybrid` (more than once).
//
// According to the standard,
//
// "Hints are provided in order of decreasing preference so, if two hints are
// contradictory, the first one controls. [...] If the same hint appears more
// than once, its second and later appearances are ignored."
//
// In practice, Chromium will only consider the first recognised hint and ignore
// the rest for the purposes of configuring the UI.
// For cases where Chromium delegates WebAuthn to the OS (e.g. Windows), unknown
// hints are filtered, but they are otherwise passed as received.
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

#if BUILDFLAG(IS_WIN)

static constexpr char kGetAssertionWithHints[] = R"((() => {
  let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
  return navigator.credentials.get({ publicKey: {
    challenge: cred_id,
    timeout: 10000,
    hints: ["nonsense", "hybrid", "security-key", "hybrid", "nonsense"],
    userVerification: 'discouraged',
    allowCredentials: [{type: 'public-key', id: cred_id}],
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

#endif  // BUILDFLAG(IS_WIN)

std::string GetSignalUnknownCredentialScript(
    base::span<const uint8_t> credential_id) {
  std::string b64_credential_id;
  base::Base64UrlEncode(credential_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &b64_credential_id);
  return base::ReplaceStringPlaceholders(kSignalUnknownCredentialId,
                                         {b64_credential_id},
                                         /*offsets=*/nullptr);
}

std::string GetSignalAllAcceptedCredentials(
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> user_id) {
  static constexpr char kSignalAllAcceptedCredentials[] = R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    allAcceptedCredentialIds: ["$1"],
    userId: "$2",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )";
  std::string b64_credential_id, b64_user_id;
  base::Base64UrlEncode(credential_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &b64_credential_id);
  base::Base64UrlEncode(user_id, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &b64_user_id);
  return base::ReplaceStringPlaceholders(kSignalAllAcceptedCredentials,
                                         {b64_credential_id, b64_user_id},
                                         /*offsets=*/nullptr);
}

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
  }

  void SetUp() override {
    https_server_.SetCertHostnames({"www.example.com"});
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

  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &WebAuthnBrowserTest::OnWillCreateBrowserContextServices,
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

  base::CallbackListSubscription subscription_;
  scoped_refptr<device::MockBluetoothAdapter> mock_bluetooth_adapter_ = nullptr;
  device::FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
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

  scoped_refptr<const extensions::Extension> extension = builder.Build();
  extensions::ExtensionRegistrar::Get(browser()->profile())
      ->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  const GURL url = extension->GetResourceURL(kPageFile);
  auto extension_id = url.GetHost();
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

class WinWebAuthnBrowserTest
    : public WebAuthnBrowserTest,
      device::WinWebAuthnApiAuthenticator::TestObserver {
 public:
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

  WinWebAuthnBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnHelloSignal,
         device::kWebAuthenticationFixWindowsHelloRdp,
         device::kWebAuthenticationWindowsHints,
         device::kWebAuthnSignalApiHidePasskeys},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    signal_unknown_credential_run_loop_ = std::make_unique<base::RunLoop>();
    signal_all_accepted_credentials_run_loop_ =
        std::make_unique<base::RunLoop>();
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory->set_discover_win_webauthn_api_authenticator(true);
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));
    device::WinWebAuthnApiAuthenticator::SetGlobalObserverForTesting(this);
  }

  void TearDownOnMainThread() override {
    device::WinWebAuthnApiAuthenticator::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::TearDownOnMainThread();
  }

  void WaitForSignalUnknownCredential() {
    signal_unknown_credential_run_loop_->Run();
    signal_unknown_credential_run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForSignalAllAcceptedCredentials() {
    signal_all_accepted_credentials_run_loop_->Run();
    signal_all_accepted_credentials_run_loop_ =
        std::make_unique<base::RunLoop>();
  }

  // device::WinWebAuthnApiAuthenticator::TestObserver:
  void OnSignalUnknownCredential() override {
    signal_unknown_credential_run_loop_->Quit();
  }

  void OnSignalAllAcceptedCredentials() override {
    signal_all_accepted_credentials_run_loop_->Quit();
  }

 protected:
  std::unique_ptr<base::RunLoop> signal_unknown_credential_run_loop_;
  std::unique_ptr<base::RunLoop> signal_all_accepted_credentials_run_loop_;
  device::FakeWinWebAuthnApi win_api_;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override_{&win_api_};
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Integration test for Large Blob on Windows.
IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest, WinLargeBlob) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  win_api_.set_version(WEBAUTHN_API_VERSION_3);

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

// Integration test for signalUnknownCredentialId on Windows.
IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest, WinSignalUnknownCredential) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  win_api_.set_version(WEBAUTHN_API_VERSION_4);
  win_api_.set_supports_silent_discovery(true);

  // Set up a Windows Hello passkey.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeDiscoverableCredential));
  ASSERT_EQ(win_api_.registrations().size(), 1u);
  const std::vector<uint8_t> credential_id =
      win_api_.registrations().begin()->first;

  // Signal the passkey as unknown, which should delete it.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      GetSignalUnknownCredentialScript(credential_id)));

  WaitForSignalUnknownCredential();
  EXPECT_TRUE(win_api_.registrations().empty());
}

// Integration test for signalAllAcceptedCredentials on Windows.
IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest,
                       WinSignalAllAcceptedCredentials) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  win_api_.set_version(WEBAUTHN_API_VERSION_4);
  win_api_.set_supports_silent_discovery(true);

  // Set up a Windows Hello passkey.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeDiscoverableCredential));
  ASSERT_EQ(win_api_.registrations().size(), 1u);
  const std::vector<uint8_t>& credential_id =
      win_api_.registrations().begin()->first;
  const std::vector<uint8_t>& user_id =
      win_api_.registrations().begin()->second.user->id;

  // Signal the passkey as known, which should keep it.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      GetSignalAllAcceptedCredentials(credential_id, user_id)));
  WaitForSignalAllAcceptedCredentials();
  EXPECT_EQ(win_api_.registrations().size(), 1u);

  // Signal a different passkey as known, which should delete the existing one.
  EXPECT_EQ("webauthn: OK",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(),
                GetSignalAllAcceptedCredentials(kCredentialID2, user_id)));
  WaitForSignalAllAcceptedCredentials();
  EXPECT_TRUE(win_api_.registrations().empty());
}

// Tests getting an assertion with an allow-list containing internal credentials
// under simulated RDP on Windows 11.
// Regression test for crbug.com/443001325.
IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest, WinGetAssertionRdp) {
  constexpr char kGetAssertionInternalCredID1234[] = R"((() => {
    let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    return navigator.credentials.get({ publicKey: {
      challenge: cred_id,
      timeout: 10000,
      userVerification: 'discouraged',
      allowCredentials: [{
        type: 'public-key',
        id: cred_id,
        transports: ['internal']
      }],
    }}).then(c => 'webauthn: OK',
            e => 'error ' + e);
  })())";

  device::fido::win::ScopedIsRdpSessionOverride rdp_override(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  win_api_.set_version(WEBAUTHN_API_VERSION_4);
  win_api_.set_is_uvpaa(true);
  win_api_.set_supports_silent_discovery(true);
  win_api_.set_simulate_rdp(true);
  win_api_.InjectNonDiscoverableCredential(kCredentialID, "www.example.com");
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionInternalCredID1234));
}

IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest, MakeCredentialHints) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  for (int version :
       std::vector{WEBAUTHN_API_VERSION_7, WEBAUTHN_API_VERSION_8}) {
    SCOPED_TRACE(version);
    win_api_.set_version(version);
    EXPECT_EQ(
        "webauthn: OK",
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        kMakeCredentialWithHints));
    if (version == WEBAUTHN_API_VERSION_7) {
      EXPECT_THAT(win_api_.last_hints(), testing::IsEmpty());
    } else {
      EXPECT_THAT(win_api_.last_hints(),
                  testing::ElementsAre(
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_HYBRID),
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_SECURITY_KEY),
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_HYBRID)));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WinWebAuthnBrowserTest, GetAssertionHints) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  win_api_.InjectNonDiscoverableCredential(kCredentialID, "www.example.com");

  for (int version :
       std::vector{WEBAUTHN_API_VERSION_7, WEBAUTHN_API_VERSION_8}) {
    SCOPED_TRACE(version);
    win_api_.set_version(version);
    EXPECT_EQ(
        "webauthn: OK",
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        kGetAssertionWithHints));
    if (version == WEBAUTHN_API_VERSION_7) {
      EXPECT_THAT(win_api_.last_hints(), testing::IsEmpty());
    } else {
      EXPECT_THAT(win_api_.last_hints(),
                  testing::ElementsAre(
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_HYBRID),
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_SECURITY_KEY),
                      testing::StrEq(WEBAUTHN_CREDENTIAL_HINT_HYBRID)));
    }
  }
}

#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest,
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
                     GetSignalUnknownCredentialScript(kCredentialID)));

  // After reporting the passkey, it should be marked as hidden.
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential =
      (passkey_model->GetPasskeyByCredentialId(
          "www.example.com",
          std::string(reinterpret_cast<const char*>(kCredentialID), 16)));
  ASSERT_TRUE(credential);
  EXPECT_TRUE(credential->hidden());
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest,
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
  EXPECT_EQ("webauthn: OK",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(),
                GetSignalAllAcceptedCredentials(kCredentialID, kUserId1)));

  // Check that the passkey with kCredentialID was not hidden.
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential =
      (passkey_model->GetPasskeyByCredentialId(
          "www.example.com",
          std::string(reinterpret_cast<const char*>(kCredentialID), 16)));
  ASSERT_TRUE(credential);
  EXPECT_FALSE(credential->hidden());

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // If the model_state is INACTIVE_STATE, it means that DeletePasskey didn't
  // run, and hence the Passkey Not Accepted bubble did not show up.
  EXPECT_EQ(model_state, password_manager::ui::INACTIVE_STATE);
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest,
                       SignalAllAcceptedCredsPasskeyDeletion) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  // Set up GPM Passkey.
  auto* passkey_model = static_cast<webauthn::TestPasskeyModel*>(
      PasskeyModelFactory::GetForProfile(browser()->profile()));
  passkey_model->AddNewPasskeyForTesting(CreateWebAuthnCredentialSpecifics(
      kCredentialID2, kUserId2, kUsername2, kDisplayName2));

  // Reports the user ID that matches the passkey created with an empty
  // allCurrentCredentialIds. The passkey will be hidden.
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

  // Check that the passkey with kCredentialID2 was hidden.
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential =
      (passkey_model->GetPasskeyByCredentialId(
          "www.example.com",
          std::string(reinterpret_cast<const char*>(kCredentialID2), 16)));
  ASSERT_TRUE(credential);
  EXPECT_TRUE(credential->hidden());

  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetState();

  // Check if the Passkey Not Accepted bubble showed up.
  EXPECT_EQ(model_state, password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE);
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ReportInvalidStrings) {
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
      content::EvalJs(
          browser()->tab_strip_model()->GetActiveWebContents(),
          base::ReplaceStringPlaceholders(kSignalUnknownCredentialId, {"A+/+A"},
                                          /*offsets=*/nullptr))
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for credentialId."));
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, SignalCurrentUserDetailsQuota) {
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

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAuthnHintsTest, HintsArePassedThrough) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialWithHints);
  observer_->WaitForHints();

  ASSERT_TRUE(observer_->hints().transport.has_value());
  EXPECT_EQ(observer_->hints().transport.value(),
            AuthenticatorTransport::kHybrid);
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
      base::BindLambdaForTesting([](device::VirtualFidoDevice* device) -> bool {
        NOTREACHED() << "Virtual device should not have been dispatched to";
      });
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);
  observer_->WaitForUI();

  // Allow the virtual device to respond to requests, then simulate clicking the
  // "Sign in with another device…" button and wait for a result.
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

class ChallengeUrlBrowserTest : public WebAuthnBrowserTest {
 public:
  static constexpr char kValidChallenge[] = "1234567890123456";

  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(ChallengeUrlBrowserTest* test_instance)
        : test_instance_(test_instance) {}
    virtual ~DelegateObserver() = default;

    void WaitForUI() {
      ui_shown_run_loop_->Run();
      ui_shown_run_loop_ = std::make_unique<base::RunLoop>();
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(delegate);
    }

    void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(nullptr);
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      ui_shown_run_loop_->QuitWhenIdle();
    }

   private:
    raw_ptr<ChallengeUrlBrowserTest> test_instance_;
    std::unique_ptr<base::RunLoop> ui_shown_run_loop_ =
        std::make_unique<base::RunLoop>();
  };
  class ModelObserver : public AuthenticatorRequestDialogModel::Observer {
   public:
    explicit ModelObserver(AuthenticatorRequestDialogModel* model)
        : model_(model) {
      model_->observers.AddObserver(this);
    }

    ~ModelObserver() override {
      if (model_) {
        model_->observers.RemoveObserver(this);
        model_ = nullptr;
      }
    }

    // Call this before the state transition you are looking to observe.
    void SetStepToObserve(AuthenticatorRequestDialogModel::Step step) {
      ASSERT_FALSE(run_loop_);
      step_ = step;
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // Call this to observer the next step change, whatever it might be.
    void ObserveNextStep() {
      ASSERT_FALSE(run_loop_);
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // This will return after a transition to the state previously specified by
    // `SetStepToObserve`. Returns immediately if the current step matches.
    void WaitForStep() {
      if (model_->step() == step_) {
        run_loop_.reset();
        return;
      }
      ASSERT_TRUE(run_loop_);
      run_loop_->Run();
      // When waiting for `kClosed` the model is deleted at this point.
      if (step_ != AuthenticatorRequestDialogModel::Step::kClosed) {
        CHECK_EQ(step_, model_->step());
      }
      Reset();
    }

    // AuthenticatorRequestDialogModel::Observer:
    void OnStepTransition() override {
      if (run_loop_ && step_ == model_->step()) {
        run_loop_->QuitWhenIdle();
      }
    }

    void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
      model_ = nullptr;
    }

    void Reset() {
      step_ = AuthenticatorRequestDialogModel::Step::kNotStarted;
      run_loop_.reset();
    }

   private:
    raw_ptr<AuthenticatorRequestDialogModel> model_;
    AuthenticatorRequestDialogModel::Step step_ =
        AuthenticatorRequestDialogModel::Step::kNotStarted;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  void SetUpOnMainThread() override {
    // Handlers have to be registered before the server is started.
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&ChallengeUrlBrowserTest::HandleChallengeRequest,
                            base::Unretained(this)));
    WebAuthnBrowserTest::SetUpOnMainThread();

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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    delegate_observer_ = std::make_unique<DelegateObserver>(this);
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        delegate_observer_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

  void SetRequestHandlerOverride(
      net::EmbeddedTestServer::HandleRequestCallback override) {
    request_handler_override_ = std::move(override);
  }

  void UpdateRequestDelegate(ChromeAuthenticatorRequestDelegate* delegate) {
    request_delegate_ = delegate;
    if (request_delegate_) {
      model_observer_ =
          std::make_unique<ModelObserver>(delegate->dialog_model());
    }
  }

  ChromeAuthenticatorRequestDelegate* request_delegate() {
    return request_delegate_;
  }

  DelegateObserver* delegate_observer() { return delegate_observer_.get(); }

  ModelObserver* model_observer() { return model_observer_.get(); }

 protected:
  static constexpr std::string kChallengePath = "/challenge";

  static constexpr char kGetAssertionWithChallengeUrl[] = R"((() => {
    let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    return navigator.credentials.get({ publicKey: {
      challengeUrl: '/challenge',
      timeout: 10000,
      userVerification: 'discouraged',
      allowCredentials: [{type: 'public-key', id: cred_id}],
      }}).then(c => { var decoder = new TextDecoder("utf-8");
                      window.domAutomationController.send(
                          decoder.decode(new Uint8Array(
                              c.response.clientDataJSON))); },
               e => window.domAutomationController.send('error ' + e));
    })())";

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleChallengeRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kChallengePath) {
      return nullptr;
    }

    if (request_handler_override_) {
      return std::move(request_handler_override_).Run(request);
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/x-webauthn-challenge");
    http_response->set_content(kValidChallenge);

    return http_response;
  }

  net::EmbeddedTestServer::HandleRequestCallback request_handler_override_;
  std::unique_ptr<DelegateObserver> delegate_observer_;
  std::unique_ptr<ModelObserver> model_observer_;
  raw_ptr<ChromeAuthenticatorRequestDelegate> request_delegate_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

IN_PROC_BROWSER_TEST_F(ChallengeUrlBrowserTest, ChallengeUrlGetAssertion) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionWithChallengeUrl);

  std::string encoded_challenge;
  base::Base64UrlEncode(kValidChallenge,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_challenge);

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_THAT(result, testing::HasSubstr(encoded_challenge));
}

// TODO(https://crbug.com/389255414): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(ChallengeUrlBrowserTest,
                       DISABLED_ChallengeUrlEmptyChallenge) {
  SetRequestHandlerOverride(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();

        http_response->set_code(net::HTTP_OK);
        http_response->set_content_type("application/x-webauthn-challenge");
        http_response->set_content("");

        return http_response;
      }));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionWithChallengeUrl);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kErrorFetchingChallenge);
  model_observer()->WaitForStep();
  request_delegate()->dialog_model()->CancelAuthenticatorRequest();

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_THAT(result, testing::HasSubstr("NotAllowedError"));
}

// TODO(https://crbug.com/389255414): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(ChallengeUrlBrowserTest,
                       DISABLED_ChallengeUrlWrongContentType) {
  SetRequestHandlerOverride(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();

        http_response->set_code(net::HTTP_OK);
        http_response->set_content_type("text/plain");
        http_response->set_content(kValidChallenge);

        return http_response;
      }));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionWithChallengeUrl);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kErrorFetchingChallenge);
  model_observer()->WaitForStep();

  request_delegate()->dialog_model()->CancelAuthenticatorRequest();

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_THAT(result, testing::HasSubstr("NotAllowedError"));
}

class WebAuthnImmediateGetTest : public WebAuthnBrowserTest {
 protected:
  static constexpr std::string_view kRequestWithPasswordTemplate = R"(
    navigator.credentials.get({
    mediation: 'immediate',
    password: $1,
    publicKey: {
      challenge: new Uint8Array([1,3,2,7,1,3,2,7]),
      timeout: 10000,
      userVerification: 'discouraged',
    }}).then(c => 'webauthn: OK', e => 'error ' + e);
  )";

  static constexpr std::string_view kRequestWithAllowlistTemplate = R"(
    navigator.credentials.get({
    mediation: 'immediate',
    publicKey: {
      challenge: new Uint8Array([1,3,2,7,1,3,2,7]),
      allowCredentials: [$1],
      timeout: 10000,
      userVerification: 'discouraged',
    }}).then(c => 'webauthn: OK', e => 'error ' + e);
  )";

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnImmediateGet};
};

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest, NoCreds_NotFoundError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (const auto& request_password : {"false", "true"}) {
    const auto& script = base::ReplaceStringPlaceholders(
        kRequestWithPasswordTemplate, {request_password},
        /*offsets=*/nullptr);
    const auto& result = content::EvalJs(web_contents, script);
    EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest,
                       Incognito_NoCreds_NotFoundError) {
  auto* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();

  for (const auto& request_password : {"false", "true"}) {
    const auto& script = base::ReplaceStringPlaceholders(
        kRequestWithPasswordTemplate, {request_password},
        /*offsets=*/nullptr);
    const auto& result = content::EvalJs(web_contents, script);
    EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest, Allowlist_NotAllowedError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const auto& script = base::ReplaceStringPlaceholders(
      kRequestWithAllowlistTemplate,
      {"{type: 'public-key', id: new Uint8Array([1,3,2,7])}"},
      /*offsets=*/nullptr);
  const auto& result = content::EvalJs(web_contents, script);
  EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
}

class WebAuthnActorBrowserTest : public WebAuthnBrowserTest {
 protected:
  static constexpr std::string_view kMakeCredentialScript = R"((() => {
    return navigator.credentials.create({ publicKey: {
      rp: { id: "www.example.com", name: "example" },
      user: { id: new Uint8Array([0]), name: "foo", displayName: "Foo" },
      pubKeyCredParams: [{type: "public-key", alg: -7}],
      challenge: new Uint8Array([0,1,2,3]),
      timeout: 10000,
    }}).then(c => 'webauthn: OK',
              e => 'error ' + e);
  })())";

 public:
  WebAuthnActorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnActorCheck, password_manager::features::kActorLogin},
        {});
  }

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
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
  }

  void CreateActingTask() {
    auto* actor_service = actor::ActorKeyedService::Get(browser()->profile());
    std::unique_ptr<actor::ExecutionEngine> execution_engine =
        std::make_unique<actor::ExecutionEngine>(browser()->profile());

    std::unique_ptr<actor::ActorTask> actor_task =
        std::make_unique<actor::ActorTask>(
            browser()->profile(), std::move(execution_engine),
            actor::ui::NewUiEventDispatcher(
                actor_service->GetActorUiStateManager()));
    actor_task->SetState(actor::ActorTask::State::kActing);

    base::RunLoop loop;
    actor_task->AddTab(
        browser()->GetActiveTabInterface()->GetHandle(),
        base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
          EXPECT_TRUE(actor::IsOk(*result));
          loop.Quit();
        }));
    loop.Run();

    actor_service->AddActiveTask(std::move(actor_task));
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnActorBrowserTest, MakeCredentialsActorIsActive) {
  CreateActingTask();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  content::EvalJsResult result =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialScript);
  EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
}

IN_PROC_BROWSER_TEST_F(WebAuthnActorBrowserTest, GetCredentialsActorIsActive) {
  CreateActingTask();
  virtual_device_factory_->mutable_state()->InjectRegistration(
      kCredentialID, "www.example.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::EvalJsResult result =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionCredID1234);
  EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
}

IN_PROC_BROWSER_TEST_F(WebAuthnActorBrowserTest,
                       GetCredentialsActorIsNotActive) {
  virtual_device_factory_->mutable_state()->InjectRegistration(
      kCredentialID, "www.example.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  // Since there is no active actor task, the request is not rejected.
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionCredID1234));
}

}  // namespace
