// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_authenticator.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

static constexpr char kRelyingPartyID[] = "example.com";

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

class TestAuthenticatorModelObserver final
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->current_step();
  }
  ~TestAuthenticatorModelObserver() override {
    if (model_) {
      model_->RemoveObserver(this);
    }
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->current_step(); }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step last_step_;
};

TEST_F(ChromeAuthenticatorRequestDelegateTest, IndividualAttestation) {
  static const struct TestCase {
    std::string name;
    std::string origin;
    std::string rp_id;
    std::string enterprise_attestation_switch_value;
    std::vector<std::string> permit_attestation_policy_values;
    bool expected;
  } kTestCases[] = {
      {"Basic", "https://login.example.com", "example.com", "", {}, false},
      {"Policy permits RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"example.com", "other.com"},
       true},
      {"Policy doesn't permit RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"other.com", "login.example.com", "https://example.com",
        "http://example.com", "https://login.example.com", "com", "*"},
       false},
      {"Policy doesn't care about the origin",
       "https://login.example.com",
       "example.com",
       "",
       {"https://login.example.com", "https://example.com"},
       false},
      {"Switch permits origin",
       "https://login.example.com",
       "example.com",
       "https://login.example.com,https://other.com,xyz:/invalidorigin",
       {},
       true},
      {"Switch doesn't permit origin",
       "https://login.example.com",
       "example.com",
       "example.com,login.example.com,http://login.example.com,https://"
       "example.com,https://a.login.example.com,https://*.example.com",
       {},
       false},
  };
  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        webauthn::switches::kPermitEnterpriseAttestationOriginList,
        test.enterprise_attestation_switch_value);
    PrefService* prefs =
        Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
    if (!test.permit_attestation_policy_values.empty()) {
      base::Value::List policy_values;
      for (const std::string& v : test.permit_attestation_policy_values)
        policy_values.Append(v);
      prefs->SetList(prefs::kSecurityKeyPermitAttestation,
                     std::move(policy_values));
    } else {
      prefs->ClearPref(prefs::kSecurityKeyPermitAttestation);
    }
    ChromeWebAuthenticationDelegate delegate;
    EXPECT_EQ(delegate.ShouldPermitIndividualAttestation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected)
        << test.name;
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, CableConfiguration) {
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    void set_cable_data(
        device::CableRequestType request_type,
        std::vector<device::CableDiscoveryData> data,
        const absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
            qr_generator_key,
        std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings)
        override {
      cable_data = std::move(data);
      qr_key = qr_generator_key;
      v2_pairings = std::move(pairings);
    }

    void set_android_accessory_params(
        mojo::Remote<device::mojom::UsbDeviceManager>,
        std::string aoa_request_description) override {
      this->aoa_configured = true;
    }

    std::vector<device::CableDiscoveryData> cable_data;
    absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>> qr_key;
    std::vector<std::unique_ptr<device::cablev2::Pairing>> v2_pairings;
    bool aoa_configured = false;
  };

  const std::array<uint8_t, 16> eid = {1, 2, 3, 4};
  const std::array<uint8_t, 32> prekey = {5, 6, 7, 8};
  const device::CableDiscoveryData v1_extension(
      device::CableDiscoveryData::Version::V1, eid, eid, prekey);

  device::CableDiscoveryData v2_extension;
  v2_extension.version = device::CableDiscoveryData::Version::V2;
  v2_extension.v2.emplace(std::vector<uint8_t>(prekey.begin(), prekey.end()),
                          std::vector<uint8_t>());

  enum class Result {
    kNone,
    kV1,
    kServerLink,
    k3rdParty,
  };

#if BUILDFLAG(IS_LINUX)
  // On Linux, some configurations aren't supported because of bluez
  // limitations. This macro maps the expected result in that case.
#define NONE_ON_LINUX(r) (Result::kNone)
#else
#define NONE_ON_LINUX(r) (r)
#endif

  const struct {
    const char* origin;
    std::vector<device::CableDiscoveryData> extensions;
    device::CableRequestType request_type;
    absl::optional<device::ResidentKeyRequirement> resident_key_requirement;
    Result expected_result;
  } kTests[] = {
      {
          "https://example.com",
          {},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v1_extension},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v2_extension},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
      },
      {
          // a.g.c should still be able to get 3rd-party caBLE
          // if it doesn't send an extension in an assertion request.
          "https://accounts.google.com",
          {},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
      },
      {
          // ... but not for non-discoverable registration.
          "https://accounts.google.com",
          {},
          device::CableRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kDiscouraged,
          Result::kNone,
      },
      {
          // ... but yes for rk=preferred
          "https://accounts.google.com",
          {},
          device::CableRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kPreferred,
          Result::k3rdParty,
      },
      {
          // ... or rk=required.
          "https://accounts.google.com",
          {},
          device::CableRequestType::kDiscoverableMakeCredential,
          device::ResidentKeyRequirement::kRequired,
          Result::k3rdParty,
      },
      {
          "https://accounts.google.com",
          {v1_extension},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          NONE_ON_LINUX(Result::kV1),
      },
      {
          "https://accounts.google.com",
          {v2_extension},
          device::CableRequestType::kGetAssertion,
          absl::nullopt,
          Result::kServerLink,
      },
  };

  unsigned test_case = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_case);
    test_case++;

    DiscoveryFactory discovery_factory;
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
    delegate.ConfigureCable(url::Origin::Create(GURL(test.origin)),
                            test.request_type, test.resident_key_requirement,
                            test.extensions, &discovery_factory);

    switch (test.expected_result) {
      case Result::kNone:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        break;

      case Result::kV1:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(delegate.dialog_model()->cable_ui_type(),
                  AuthenticatorRequestDialogModel::CableUIType::CABLE_V1);
        break;

      case Result::kServerLink:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type(),
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK);
        break;

      case Result::k3rdParty:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type(),
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR);
        break;
    }
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ConditionalUI) {
  // The RenderFrame has to be live for the ChromeWebAuthnCredentialsDelegate to
  // be created.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());

  // Enabling conditional mode should cause the modal dialog to stay hidden at
  // the beginning of a request. An omnibar icon might be shown instead.
  for (bool conditional_ui : {true, false}) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetConditionalRequest(conditional_ui);
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    AuthenticatorRequestDialogModel* model = delegate.dialog_model();
    TestAuthenticatorModelObserver observer(model);
    model->AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    delegate.OnTransportAvailabilityEnumerated(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo());
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kConditionalMediation,
              conditional_ui);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest) {
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  static const struct {
    std::string rp_id;
    std::string origin;
    bool expected;
  } kTests[] = {
      {"example.com", "https://example.com", false},
      {"foo.com", "https://example.com", false},
      {"abcdef", kTestExtensionOrigin, true},
      {"abcdefg", kTestExtensionOrigin, false},
      {"example.com", kTestExtensionOrigin, false},
  };

  ChromeWebAuthenticationDelegate delegate;
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, MaybeGetRelyingPartyIdOverride) {
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  ChromeWebAuthenticationDelegate delegate;
  static const struct {
    std::string rp_id;
    std::string origin;
    absl::optional<std::string> expected;
  } kTests[] = {
      {"example.com", "https://example.com", absl::nullopt},
      {"foo.com", "https://example.com", absl::nullopt},
      {"abcdef", kTestExtensionOrigin, kTestExtensionOrigin},
      {"example.com", kTestExtensionOrigin, kTestExtensionOrigin},
  };
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.MaybeGetRelyingPartyIdOverride(
                  test.rp_id, url::Origin::Create(GURL(test.origin))),
              test.expected);
  }
}

// Tests that attestation is returned if the virtual environment is enabled and
// the UI is disabled.
// Regression test for crbug.com/1342458
TEST_F(ChromeAuthenticatorRequestDelegateTest, VirtualEnvironmentAttestation) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.DisableUI();
  delegate.SetVirtualEnvironment(true);
  device::VirtualFidoDeviceAuthenticator authenticator(
      std::make_unique<device::VirtualCtap2Device>());
  device::test::ValueCallbackReceiver<bool> cb;
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  EXPECT_TRUE(cb.value());
}

#if BUILDFLAG(IS_MAC)
std::string TouchIdMetadataSecret(ChromeWebAuthenticationDelegate& delegate,
                                  content::BrowserContext* browser_context) {
  return delegate.GetTouchIdAuthenticatorConfig(browser_context)
      ->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeWebAuthenticationDelegate delegate;
  std::string secret = TouchIdMetadataSecret(delegate, GetBrowserContext());
  EXPECT_EQ(secret.size(), 32u);
  // The secret should be stable.
  EXPECT_EQ(secret, TouchIdMetadataSecret(delegate, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return
  // the same secret.
  ChromeWebAuthenticationDelegate delegate1;
  ChromeWebAuthenticationDelegate delegate2;
  EXPECT_EQ(TouchIdMetadataSecret(delegate1, GetBrowserContext()),
            TouchIdMetadataSecret(delegate2, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets.
  auto other_browser_context = CreateBrowserContext();
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_NE(TouchIdMetadataSecret(delegate, GetBrowserContext()),
            TouchIdMetadataSecret(delegate, other_browser_context.get()));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(
      32u, TouchIdMetadataSecret(delegate, other_browser_context.get()).size());
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

// Tests that ShouldReturnAttestation() returns with true if |authenticator|
// is the Windows native WebAuthn API with WEBAUTHN_API_VERSION_2 or higher,
// where Windows prompts for attestation in its own native UI.
//
// Ideally, this would also test the inverse case, i.e. that with
// WEBAUTHN_API_VERSION_1 Chrome's own attestation prompt is shown. However,
// there seems to be no good way to test AuthenticatorRequestDialogModel UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, ShouldPromptForAttestationWin) {
  ::device::FakeWinWebAuthnApi win_webauthn_api;
  win_webauthn_api.set_version(WEBAUTHN_API_VERSION_2);
  ::device::WinWebAuthnApiAuthenticator authenticator(
      /*current_window=*/nullptr, &win_webauthn_api);

  ::device::test::ValueCallbackReceiver<bool> cb;
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), true);
}

#endif  // BUILDFLAG(IS_WIN)

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeAuthenticatorRequestDelegateTest {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kExampleOrigin[] = "https://example.com";

  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege};
};

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       RemoteProxiedRequestsAllowedPolicy) {
  // The "webauthn.remote_proxied_requests_allowed" policy pref should enable
  // Google's internal CRD origin to use the RemoteDesktopClientOverride
  // extension.
  enum class Policy {
    kUnset,
    kDisabled,
    kEnabled,
  };
  ChromeWebAuthenticationDelegate delegate;
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  for (auto* origin : {kCorpCrdOrigin, kExampleOrigin}) {
    for (const auto policy :
         {Policy::kUnset, Policy::kDisabled, Policy::kEnabled}) {
      switch (policy) {
        case Policy::kUnset:
          prefs->ClearPref(webauthn::pref_names::kRemoteProxiedRequestsAllowed);
          break;
        case Policy::kDisabled:
          prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                            false);
          break;
        case Policy::kEnabled:
          prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                            true);
          break;
      }

      EXPECT_EQ(delegate.OriginMayUseRemoteDesktopClientOverride(
                    browser_context(), url::Origin::Create(GURL(origin))),
                origin == kCorpCrdOrigin && policy == Policy::kEnabled);
    }
  }
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       OriginMayUseRemoteDesktopClientOverrideAdditionalOriginSwitch) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing.
  ChromeWebAuthenticationDelegate delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  // The flag shouldn't have an effect without the policy enabled.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kCorpCrdOrigin))));

  // With the policy enabled, both the hard-coded and flag origin should be
  // allowed.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed, true);
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kCorpCrdOrigin))));

  // Other origins still shouldn't be permitted.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://other.example.com"))));
}

class DisableWebAuthnWithBrokenCertsTest
    : public ChromeAuthenticatorRequestDelegateTest {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kDisableWebAuthnWithBrokenCerts};
};

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelNotAcceptable) {
  GURL url("https://doofenshmirtz.evil");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_FALSE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, ExtensionSupported) {
  GURL url("chrome-extension://extensionid");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, EnterpriseOverride) {
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetBoolean(webauthn::pref_names::kAllowWithBrokenCerts, true);
  GURL url("https://doofenshmirtz.evil");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, Localhost) {
  GURL url("http://localhost");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelAcceptable) {
  GURL url("https://owca.org");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = 0;  // ok.
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

}  // namespace
