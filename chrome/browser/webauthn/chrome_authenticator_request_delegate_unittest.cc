// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_authenticator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/testing_profile.h"
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

static constexpr char kCredentialId1[] = "credential_id_1";
static constexpr char kCredentialId2[] = "credential_id_2";
static constexpr char kUserId[] = "hmiku-userid";
static constexpr char kUserName1[] = "hmiku";
static constexpr char kUserDisplayName1[] = "Hatsune Miku";
static constexpr char kUserName2[] = "reimu";
static constexpr char kUserDisplayName2[] = "Reimu Hakurei";
static constexpr char kRpId[] = "example.com";

using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;

std::vector<uint8_t> ToByteVector(std::string_view string) {
  return std::vector<uint8_t>(string.begin(), string.end());
}

class Observer : public testing::NiceMock<
                     ChromeAuthenticatorRequestDelegate::TestObserver> {
 public:
  MOCK_METHOD(void,
              Created,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(std::vector<std::unique_ptr<device::cablev2::Pairing>>,
              GetCablePairingsFromSyncedDevices,
              (),
              (override));
  MOCK_METHOD(void,
              OnTransportAvailabilityEnumerated,
              (ChromeAuthenticatorRequestDelegate * delegate,
               TransportAvailabilityInfo* tai),
              (override));
  MOCK_METHOD(void,
              UIShown,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(void,
              CableV2ExtensionSeen,
              (base::span<const uint8_t> server_link_data),
              (override));
};

class MockCableDiscoveryFactory : public device::FidoDiscoveryFactory {
 public:
  void set_cable_data(
      device::FidoRequestType request_type,
      std::vector<device::CableDiscoveryData> data,
      const std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
          qr_generator_key) override {
    cable_data = std::move(data);
    qr_key = qr_generator_key;
  }

  std::vector<device::CableDiscoveryData> cable_data;
  std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>> qr_key;
};

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeAuthenticatorRequestDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        {syncer::kSyncWebauthnCredentials, syncer::kSyncWebauthnCredentials},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);
  }

  void TearDown() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  Observer observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TestAuthenticatorModelObserver final
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->step();
  }
  ~TestAuthenticatorModelObserver() override {
    if (model_) {
      model_->observers.RemoveObserver(this);
    }
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->step(); }

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
      for (const std::string& v : test.permit_attestation_policy_values) {
        policy_values.Append(v);
      }
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
    device::FidoRequestType request_type;
    std::optional<device::ResidentKeyRequirement> resident_key_requirement;
    Result expected_result;
    // expected_result_with_system_hybrid is the behaviour that should occur
    // when the operating system supports hybrid itself. (I.e. recent versions
    // of Windows.)
    Result expected_result_with_system_hybrid;
  } kTests[] = {
      {
          "https://example.com",
          {},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // a.g.c should still be able to get 3rd-party caBLE
          // if it doesn't send an extension in an assertion request.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // ... but not for non-discoverable registration.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kDiscouraged,
          Result::kNone,
          Result::kNone,
      },
      {
          // ... but yes for rk=preferred
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kPreferred,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // ... or rk=required.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kRequired,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          "https://accounts.google.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          NONE_ON_LINUX(Result::kV1),
          Result::kV1,
      },
      {
          "https://accounts.google.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::kServerLink,
          Result::kServerLink,
      },
  };

  // On Windows, all the tests are run twice. Once to check that, when Windows
  // has hybrid support, it's not also configured in Chrome, and again to test
  // the prior behaviour.

#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
#endif

  enum WinHybridExpectation {
    kNoWinHybrid,
    kWinHybridPasskeySyncing,
    kWinHybridNoPasskeySyncing,
  };

  for (const WinHybridExpectation windows_has_hybrid : {
           kNoWinHybrid,
#if BUILDFLAG(IS_WIN)
           kWinHybridPasskeySyncing,
           kWinHybridNoPasskeySyncing,
#endif
       }) {
    unsigned test_case = 0;
    for (const auto& test : kTests) {
      SCOPED_TRACE(test_case);
      test_case++;

#if BUILDFLAG(IS_WIN)
      fake_win_webauthn_api.set_version(windows_has_hybrid == kNoWinHybrid ? 4
                                                                           : 7);
      base::test::ScopedFeatureList scoped_feature_list;
      if (windows_has_hybrid == kWinHybridNoPasskeySyncing) {
        scoped_feature_list.InitWithFeatures(
            {}, {syncer::kSyncWebauthnCredentials});
      } else if (windows_has_hybrid == kWinHybridPasskeySyncing) {
        scoped_feature_list.InitWithFeatures({syncer::kSyncWebauthnCredentials},
                                             {});
      }
      SCOPED_TRACE(windows_has_hybrid);
#endif

      MockCableDiscoveryFactory discovery_factory;
      ChromeAuthenticatorRequestDelegate delegate(main_rfh());
      delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
      delegate.ConfigureDiscoveries(
          url::Origin::Create(GURL(test.origin)), test.origin,
          content::AuthenticatorRequestClientDelegate::RequestSource::
              kWebAuthentication,
          test.request_type, test.resident_key_requirement,
          device::UserVerificationRequirement::kRequired,
          /*user_name=*/std::nullopt, test.extensions,
          /*is_enclave_authenticator_available=*/false, &discovery_factory);

      switch (windows_has_hybrid == kWinHybridNoPasskeySyncing
                  ? test.expected_result_with_system_hybrid
                  : test.expected_result) {
        case Result::kNone:
          EXPECT_FALSE(discovery_factory.qr_key.has_value());
          EXPECT_TRUE(discovery_factory.cable_data.empty());
          break;

        case Result::kV1:
          EXPECT_FALSE(discovery_factory.qr_key.has_value());
          EXPECT_FALSE(discovery_factory.cable_data.empty());
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type,
                    AuthenticatorRequestDialogModel::CableUIType::CABLE_V1);
          break;

        case Result::kServerLink:
          EXPECT_TRUE(discovery_factory.qr_key.has_value());
          EXPECT_FALSE(discovery_factory.cable_data.empty());
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type,
                    AuthenticatorRequestDialogModel::CableUIType::
                        CABLE_V2_SERVER_LINK);
          break;

        case Result::k3rdParty:
          EXPECT_TRUE(discovery_factory.qr_key.has_value());
          EXPECT_TRUE(discovery_factory.cable_data.empty());
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type,
                    AuthenticatorRequestDialogModel::CableUIType::
                        CABLE_V2_2ND_FACTOR);
          break;
      }
    }
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, NoExtraDiscoveriesWithoutUI) {
  const std::string rp_id = "https://example.com";
  const std::string origin = "https://" + rp_id;

  for (const bool disable_ui : {false, true}) {
    SCOPED_TRACE(disable_ui);

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(rp_id);
    if (disable_ui) {
      delegate.DisableUI();
    }
    MockCableDiscoveryFactory discovery_factory;
    delegate.ConfigureDiscoveries(
        url::Origin::Create(GURL(origin)), origin,
        content::AuthenticatorRequestClientDelegate::RequestSource::
            kWebAuthentication,
        device::FidoRequestType::kMakeCredential,
        device::ResidentKeyRequirement::kPreferred,
        device::UserVerificationRequirement::kRequired,
        /*user_name=*/std::nullopt, {},
        /*is_enclave_authenticator_available=*/false, &discovery_factory);

    EXPECT_EQ(discovery_factory.qr_key.has_value(), !disable_ui);

    // `discovery_factory.nswindow_` won't be set in any case because it depends
    // on the `RenderFrameHost` having a `BrowserWindow`, which it doesn't in
    // this context.
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
    model->observers.AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    delegate.OnTransportAvailabilityEnumerated(std::move(transports_info));
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kConditionalMediation,
              conditional_ui);
  }
}

constexpr char kExtensionId[] = "extension-id";
constexpr char kExtensionOrigin[] = "chrome-extension://extension-id";

typedef struct {
  const char* pattern;
  const char* rp_id;
} PatternRpIdPair;

constexpr PatternRpIdPair kValidRelyingPartyTestCases[] = {
    // Extensions are always allowed to claim their own origins.
    {"", kExtensionId},

    {"<all_urls>", "google.com"},
    {"https://*/*", "google.com"},
    {"https://*.google.com/", "google.com"},
    {"https://*.subdomain.google.com/", "google.com"},

    // The rules below are a sanity check to verify that the implementation
    // matches webauthn rules and are copied from
    // content/browser/webauth/authenticator_impl_unittest.cc.
    {"http://localhost/", "localhost"},
    {"https://foo.bar.google.com/", "foo.bar.google.com"},
    {"https://foo.bar.google.com/", "bar.google.com"},
    {"https://foo.bar.google.com/", "google.com"},
    {"https://earth.login.awesomecompany/", "login.awesomecompany"},
    {"https://google.com:1337/", "google.com"},
    {"https://google.com./", "google.com"},
    {"https://google.com./", "google.com."},
    {"https://google.com../", "google.com.."},
    {"https://.google.com/", "google.com"},
    {"https://..google.com/", "google.com"},
    {"https://.google.com/", ".google.com"},
    {"https://..google.com/", ".google.com"},
    {"https://accounts.google.com/", ".google.com"},
};

constexpr PatternRpIdPair kInvalidRelyingPartyTestCases[] = {
    // Extensions are not allowed to claim RP IDs belonging to other extensions.
    {"chrome-extension://some-other-extension/",
     "chrome-extension://some-other-extension/"},
    {"chrome-extension://some-other-extension/", "some-other-extension"},

    // Extensions are not allowed to claim RP IDs matching eTLDs, even if they
    // have host permissions over their origins.
    {"<all_urls>", "com"},
    {"https://*/*", "com"},
    {"https://com/", "com"},

    // Single component domains are considered eTLDs, even if not on the PSL.
    {"https://myawesomedomain/", "myawesomedomain"},

    // The rules below are a sanity check to verify that the implementation
    // matches webauthn rules and are copied from
    // content/browser/webauth/authenticator_impl_unittest.cc.
    {"https://google.com/", "com"},
    {"http://google.com/", "google.com"},
    {"http://myawesomedomain/", "myawesomedomain"},
    {"https://google.com/", "foo.bar.google.com"},
    {"http://myawesomedomain/", "randomdomain"},
    {"https://myawesomedomain/", "randomdomain"},
    {"https://notgoogle.com/", "google.com)"},
    {"https://not-google.com/", "google.com)"},
    {"https://evil.appspot.com/", "appspot.com"},
    {"https://evil.co.uk/", "co.uk"},
    // TODO(nsatragno): URLPattern erroneously trims trailing dots. Fix
    // CanonicalizeHostForMatching and uncomment this line.
    // {"https://google.com/", "google.com."},
    {"https://google.com/", "google.com.."},
    {"https://google.com/", ".google.com"},
    {"https://google.com../", "google.com"},
    {"https://.com/", "com."},
    {"https://.co.uk/", "co.uk."},
    {"https://1.2.3/", "1.2.3"},
    {"https://1.2.3/", "2.3"},
    {"https://127.0.0.1/", "127.0.0.1"},
    {"https://127.0.0.1/", "27.0.0.1"},
    {"https://127.0.0.1/", ".0.0.1"},
    {"https://127.0.0.1/", "0.0.1"},
    {"https://[::127.0.0.1]/", "127.0.0.1"},
    {"https://[::127.0.0.1]/", "[127.0.0.1]"},
    {"https://[::1]/", "1"},
    {"https://[::1]/", "1]"},
    {"https://[::1]/", "::1"},
    {"https://[::1]/", "[::1]"},
    {"https://[1::1]/", "::1"},
    {"https://[1::1]/", "::1]"},
    {"https://[1::1]/", "[::1]"},
    {"http://google.com:443/", "google.com"},
    {"data:google.com/", "google.com"},
    {"data:text/html,google.com/", "google.com"},
    {"ws://google.com/", "google.com"},
    {"ftp://google.com/", "google.com"},
    {"file://google.com/", "google.com"},
    {"wss://google.com/", "google.com"},
    {"data:,/", ""},
    {"https://google.com/", ""},
    {"ws://google.com/", ""},
    {"wss://google.com/", ""},
    {"ftp://google.com/", ""},
    {"file://google.com/", ""},
    {"https://login.awesomecompany/", "awesomecompany"},
};

// Tests that an extension origin can claim relying party IDs it has permissions
// for.
TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionValidCases) {
  for (const auto& test : kValidRelyingPartyTestCases) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension name")
            .SetID(kExtensionId)
            .AddHostPermission(test.pattern)
            .Build();
    extensions::ExtensionRegistry::Get(browser_context())
        ->AddEnabled(extension);

    ChromeWebAuthenticationDelegate delegate;
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message() << "pattern=" << test.pattern);
    EXPECT_TRUE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
        GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
        test.rp_id));
  }
}

// Tests that an extension origin cannot claim relying party IDs it does not
// have permissions for.
TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionInvalidCases) {
  for (const auto& test : kInvalidRelyingPartyTestCases) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension name")
            .SetID(kExtensionId)
            .AddHostPermission(test.pattern)
            .Build();
    extensions::ExtensionRegistry::Get(browser_context())
        ->AddEnabled(extension);

    ChromeWebAuthenticationDelegate delegate;
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message() << "pattern=" << test.pattern);
    EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
        GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
        test.rp_id));
  }
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// chrome-extension origins that don't match an active extension.
TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionNotFound) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
      kExtensionId));
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// web origins.
TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_WebOrigin) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL("https://google.com")),
      kExtensionId));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, MaybeGetRelyingPartyIdOverride) {
  ChromeWebAuthenticationDelegate delegate;
  static const struct {
    std::string rp_id;
    std::string origin;
    std::optional<std::string> expected;
  } kTests[] = {
      {"example.com", "https://example.com", std::nullopt},
      {"foo.com", "https://example.com", std::nullopt},
      {"example.com", kExtensionOrigin, std::nullopt},
      {kExtensionId, kExtensionOrigin, kExtensionOrigin},
  };
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.MaybeGetRelyingPartyIdOverride(
                  test.rp_id, url::Origin::Create(GURL(test.origin))),
              test.expected);
  }
}

// Tests that synced GPM passkeys are injected in the transport availability
// info.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys) {
  std::string relying_party = "example.com";
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(relying_party);

  // Set up a paired phone from sync.
  auto phone = std::make_unique<device::cablev2::Pairing>();
  phone->name = "Miku's Pixel 7 XL";
  phone->contact_id = {1, 2, 3, 4};
  phone->id = {5, 6, 7, 8};
  phone->from_sync_deviceinfo = true;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(std::move(phone));
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices)
      .WillOnce(testing::Return(testing::ByMove(std::move(phones))));
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), relying_party,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/std::nullopt,
      device::UserVerificationRequirement::kRequired,
      /*user_name=*/std::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      /*is_enclave_authenticator_available=*/false, &discovery_factory);

  // Add a synced passkey for example.com and another for othersite.com.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id(kRpId);
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey.set_user_name(kUserName1);
  passkey.set_user_display_name(kUserDisplayName1);

  sync_pb::WebauthnCredentialSpecifics passkey_other_rp_id = passkey;
  passkey_other_rp_id.set_rp_id("othersite.com");

  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  passkey_model->AddNewPasskeyForTesting(std::move(passkey_other_rp_id));

  TransportAvailabilityInfo tai;
  tai.request_type = device::FidoRequestType::kGetAssertion;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey for example.com should have been added to the recognized
  // credentials list.
  ASSERT_EQ(tai.recognized_credentials.size(), 1u);
  const device::DiscoverableCredentialMetadata credential =
      tai.recognized_credentials.at(0);
  EXPECT_EQ(credential.cred_id, std::vector<uint8_t>(16, 'b'));
  EXPECT_EQ(credential.rp_id, kRpId);
  EXPECT_EQ(credential.source, device::AuthenticatorType::kPhone);
  EXPECT_EQ(credential.user.display_name, kUserDisplayName1);
  EXPECT_EQ(credential.user.name, kUserName1);
  EXPECT_EQ(credential.user.id, std::vector<uint8_t>({5, 6, 7, 8}));
}

// Tests that synced GPM passkeys are not discovered if there are no sync paired
// phones.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys_NoSyncPairedPhones) {
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(kRpId);

  // Return an empty list of synced devices.
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices);
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), kRpId,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/std::nullopt,
      device::UserVerificationRequirement::kRequired,
      /*user_name=*/std::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      /*is_enclave_authenticator_available=*/false, &discovery_factory);

  // Add a synced passkey for example.com.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id(kRpId);
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey_model->AddNewPasskeyForTesting(std::move(passkey));

  TransportAvailabilityInfo tai;
  tai.request_type = device::FidoRequestType::kGetAssertion;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey should not be present in the recognized credentials list.
  EXPECT_TRUE(tai.recognized_credentials.empty());
}

// Tests that shadowed GPM passkeys are not discovered.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys_ShadowedPasskeys) {
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(kRpId);

  // Set up a paired phone from sync.
  auto phone = std::make_unique<device::cablev2::Pairing>();
  phone->name = "Miku's Pixel 7 XL";
  phone->contact_id = {1, 2, 3, 4};
  phone->id = {5, 6, 7, 8};
  phone->from_sync_deviceinfo = true;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(std::move(phone));
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices)
      .WillOnce(testing::Return(testing::ByMove(std::move(phones))));
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), kRpId,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/std::nullopt,
      device::UserVerificationRequirement::kRequired,
      /*user_name=*/std::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      /*is_enclave_authenticator_available=*/false, &discovery_factory);

  // Add a synced passkey for example.com and another that shadows it.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id(kRpId);
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey.set_user_name(kUserName1);
  passkey.set_user_display_name(kUserDisplayName1);

  sync_pb::WebauthnCredentialSpecifics shadowed_passkey = passkey;
  shadowed_passkey.set_credential_id(std::string(16, 'c'));
  passkey.add_newly_shadowed_credential_ids(shadowed_passkey.credential_id());

  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  passkey_model->AddNewPasskeyForTesting(std::move(shadowed_passkey));

  TransportAvailabilityInfo tai;
  tai.request_type = device::FidoRequestType::kGetAssertion;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey that is not shadowed should have been added to the
  // recognized credentials list.
  ASSERT_EQ(tai.recognized_credentials.size(), 1u);
  const device::DiscoverableCredentialMetadata credential =
      tai.recognized_credentials.at(0);
  EXPECT_EQ(credential.cred_id, std::vector<uint8_t>(16, 'b'));
  EXPECT_EQ(credential.rp_id, kRpId);
  EXPECT_EQ(credential.source, device::AuthenticatorType::kPhone);
  EXPECT_EQ(credential.user.display_name, kUserDisplayName1);
  EXPECT_EQ(credential.user.name, kUserName1);
  EXPECT_EQ(credential.user.id, std::vector<uint8_t>({5, 6, 7, 8}));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, FilterGoogleComPasskeys) {
  auto HasCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kHasRecognizedCredential;
  auto NoCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kNoRecognizedCredential;
  auto UnknownCreds =
      device::FidoRequestHandlerBase::RecognizedCredential::kUnknown;
  constexpr char kGoogle[] = "google.com";
  constexpr char kOtherRpId[] = "example.com";
  struct {
    std::string rp_id;
    device::FidoRequestHandlerBase::RecognizedCredential recognized_credential;
    std::vector<std::string> user_ids;

    device::FidoRequestHandlerBase::RecognizedCredential
        expected_recognized_credential;
    std::vector<std::string> expected_user_ids;
  } kTests[] = {
      {kOtherRpId,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"}},
      {kGoogle,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2", "AUTOFILL_AUTH:c3"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1"}},
      {kGoogle, HasCreds, {"c2", "AUTOFILL_AUTH:c3"}, NoCreds, {}},
      {kGoogle, UnknownCreds, {}, UnknownCreds, {}},
      {kGoogle, HasCreds, {}, HasCreds, {}},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message()
                 << "creds=" << base::JoinString(test.user_ids, ","));
    TransportAvailabilityInfo data;
    data.has_empty_allow_list = true;
    data.request_type = device::FidoRequestType::kGetAssertion;
    TransportAvailabilityInfo result;
    EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
        .WillOnce([&result](const auto* _, const auto* new_tai) {
          result = std::move(*new_tai);
        });

    for (const std::string& user_id : test.user_ids) {
      data.recognized_credentials.emplace_back(
          device::AuthenticatorType::kOther, test.rp_id,
          std::vector<uint8_t>{0},
          device::PublicKeyCredentialUserEntity(
              std::vector<uint8_t>(user_id.begin(), user_id.end())));
    }
    data.has_platform_authenticator_credential = test.recognized_credential;

    // Mix in an icloud keychain credential. These should not be filtered or
    // affect setting the recognized credentials flag.
    data.recognized_credentials.emplace_back(
        device::AuthenticatorType::kICloudKeychain, test.rp_id,
        std::vector<uint8_t>{0}, device::PublicKeyCredentialUserEntity({1}));
    data.has_icloud_keychain_credential = device::FidoRequestHandlerBase::
        RecognizedCredential::kHasRecognizedCredential;

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(test.rp_id);
    delegate.RegisterActionCallbacks(base::DoNothing(), base::DoNothing(),
                                     base::DoNothing(), base::DoNothing(),
                                     base::DoNothing(), base::DoNothing());
    delegate.OnTransportAvailabilityEnumerated(std::move(data));

    EXPECT_EQ(result.has_platform_authenticator_credential,
              test.expected_recognized_credential);
    EXPECT_EQ(result.has_icloud_keychain_credential,
              device::FidoRequestHandlerBase::RecognizedCredential::
                  kHasRecognizedCredential);
    ASSERT_EQ(result.recognized_credentials.size(),
              test.expected_user_ids.size() + 1);
    for (size_t i = 0; i < test.expected_user_ids.size(); ++i) {
      std::string expected_id = test.expected_user_ids[i];
      EXPECT_EQ(result.recognized_credentials[i].user.id,
                std::vector<uint8_t>(expected_id.begin(), expected_id.end()));
    }
    EXPECT_EQ(result.recognized_credentials.back().source,
              device::AuthenticatorType::kICloudKeychain);
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       FilterGoogleComPasskeysWithNonEmptyAllowList) {
  // Regression test for crbug.com/40071851, b/366128135.
  constexpr char kGoogleRpId[] = "google.com";
  TransportAvailabilityInfo data;
  data.has_empty_allow_list = false;
  data.request_type = device::FidoRequestType::kGetAssertion;
  TransportAvailabilityInfo result;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&result](const auto* _, const auto* new_tai) {
        result = std::move(*new_tai);
      });

  // User ID doesn't start with the `GOOGLE_ACCOUNT:` prefix that distinguishes
  // them as suitable for login auth.
  std::string user_id = "test user id";
  data.recognized_credentials.emplace_back(
      device::AuthenticatorType::kOther, kGoogleRpId, std::vector<uint8_t>{0},
      device::PublicKeyCredentialUserEntity(
          std::vector<uint8_t>(user_id.begin(), user_id.end())));
  data.has_platform_authenticator_credential = device::FidoRequestHandlerBase::
      RecognizedCredential::kHasRecognizedCredential;

  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(kGoogleRpId);
  delegate.RegisterActionCallbacks(base::DoNothing(), base::DoNothing(),
                                   base::DoNothing(), base::DoNothing(),
                                   base::DoNothing(), base::DoNothing());
  delegate.OnTransportAvailabilityEnumerated(std::move(data));

  // Despite lacking the user ID prefix, credentials are not filtered from
  // `recognized_credentials` because the request has a non-empty allow list.
  // The RecognizedCredential status isn't adjusted either.
  ASSERT_EQ(result.recognized_credentials.size(), 1u);
  EXPECT_EQ(result.has_platform_authenticator_credential,
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential);
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, DeletePasskey) {
  ChromeWebAuthenticationDelegate delegate;
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id(kCredentialId1);
  passkey.set_rp_id(kRpId);
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  {
    // Attempt removing an unknown credential.
    base::HistogramTester histogram_tester;
    delegate.DeletePasskey(web_contents(), ToByteVector(kCredentialId2), kRpId);
    EXPECT_TRUE(passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1));
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
            kPasskeyNotFound,
        1);
  }
  {
    // Remove a known credential.
    base::HistogramTester histogram_tester;
    delegate.DeletePasskey(web_contents(), ToByteVector(kCredentialId1), kRpId);
    EXPECT_FALSE(
        passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1));
    histogram_tester.ExpectBucketCount(
        "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
            kPasskeyRemoved,
        1);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, DeleteUnacceptedPasskey) {
  ChromeWebAuthenticationDelegate delegate;
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id(kCredentialId1);
  passkey.set_rp_id(kRpId);
  passkey.set_user_id(kUserId);
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  {
    // Pass a known credential. It should not be removed.
    base::HistogramTester histogram_tester;
    delegate.DeleteUnacceptedPasskeys(web_contents(), kRpId,
                                      ToByteVector(kUserId),
                                      {ToByteVector(kCredentialId1)});
    EXPECT_TRUE(passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1));
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kNoPasskeyRemoved,
        1);
  }
  {
    // Do not pass the known credential. The known credential should be removed.
    base::HistogramTester histogram_tester;
    delegate.DeleteUnacceptedPasskeys(web_contents(), kRpId,
                                      ToByteVector(kUserId),
                                      {ToByteVector(kCredentialId2)});
    EXPECT_FALSE(
        passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1));
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kPasskeyRemoved,
        1);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, UpdatePasskey) {
  const auto test_origin = url::Origin::Create(GURL("https://example.com"));
  std::vector<uint8_t> user_id = ToByteVector(kUserId);
  ChromeWebAuthenticationDelegate delegate;
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  {
    sync_pb::WebauthnCredentialSpecifics passkey;
    passkey.set_credential_id(kCredentialId1);
    passkey.set_rp_id(kRpId);
    passkey.set_user_id(kUserId);
    passkey.set_user_name(kUserName1);
    passkey.set_user_display_name(kUserDisplayName1);
    passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  }
  {
    // Setting the same username/display name should not result in an update.
    base::HistogramTester histogram_tester;
    delegate.UpdateUserPasskeys(web_contents(), test_origin, kRpId, user_id,
                                kUserName1, kUserDisplayName1);
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalCurrentUserDetailsUpdatedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalCurrentUserDetailsResult::
            kPasskeyNotUpdated,
        1);
  }
  {
    // Setting a different username/display name should result in an update.
    base::HistogramTester histogram_tester;
    delegate.UpdateUserPasskeys(web_contents(), test_origin, kRpId, user_id,
                                kUserName2, kUserDisplayName2);
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalCurrentUserDetailsUpdatedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalCurrentUserDetailsResult::
            kPasskeyUpdated,
        1);
    sync_pb::WebauthnCredentialSpecifics passkey =
        *passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1);
    EXPECT_EQ(kUserName2, passkey.user_name());
    EXPECT_EQ(kUserDisplayName2, passkey.user_display_name());
  }
  {
    // Exceed the quota and try updating a passkey.
    for (int i = 0; i < webauthn::PasskeyChangeQuotaTracker::kMaxTokensPerRP;
         ++i) {
      delegate.UpdateUserPasskeys(web_contents(), test_origin, kRpId, user_id,
                                  base::RandBytesAsString(8),
                                  base::RandBytesAsString(8));
    }
    base::HistogramTester histogram_tester;
    delegate.UpdateUserPasskeys(web_contents(), test_origin, kRpId, user_id,
                                kUserName1, kUserDisplayName1);
    sync_pb::WebauthnCredentialSpecifics passkey =
        *passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1);
    EXPECT_NE(kUserName1, passkey.user_name());
    EXPECT_NE(kUserDisplayName1, passkey.user_display_name());
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalCurrentUserDetailsUpdatedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalCurrentUserDetailsResult::
            kQuotaExceeded,
        1);
  }
}

class EnclaveAuthenticatorRequestDelegateTest
    : public ChromeAuthenticatorRequestDelegateTest {
 public:
  void SetUp() override {
    ChromeAuthenticatorRequestDelegateTest::SetUp();
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }
};

// ChromeOS delegates this logic to a ChromeOS-specific service.

#if !BUILDFLAG(IS_CHROMEOS)

TEST_F(EnclaveAuthenticatorRequestDelegateTest,
       BrowserProvidedPasskeysAvailable) {
  struct {
    bool is_flag_enabled;
    bool has_consented_account;
    bool is_syncing_passwords;
    bool has_unexportable_keys;
    bool expected_passkeys_available;
  } kTestCases[] = {
      // flag acc   sync  unexp result   flag   acc   sync  unexp result
      {true, true, true, true, true},   {false, true, true, true, false},
      {true, false, true, true, false}, {true, true, false, true, false},
      {true, true, true, false, false},
  };
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "is_flag_enabled=" << test.is_flag_enabled);
    SCOPED_TRACE(testing::Message()
                 << "has_consented_account=" << test.has_consented_account);
    SCOPED_TRACE(testing::Message()
                 << "is_syncing_passwords=" << test.is_syncing_passwords);
    SCOPED_TRACE(testing::Message()
                 << "has_unexportable_keys=" << test.has_unexportable_keys);
    ChromeWebAuthenticationDelegate delegate;
    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitWithFeatureState(
        device::kWebAuthnEnclaveAuthenticator, test.is_flag_enabled);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    if (test.has_consented_account) {
      signin::MakePrimaryAccountAvailable(identity_manager,
                                          "hikari@example.com",
                                          signin::ConsentLevel::kSignin);
    }

    auto* test_sync_service = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile()));
    test_sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPasswords, test.is_syncing_passwords);

    absl::variant<crypto::ScopedNullUnexportableKeyProvider,
                  crypto::ScopedMockUnexportableKeyProvider>
        unexportable_key_provider;
    if (test.has_unexportable_keys) {
      unexportable_key_provider
          .emplace<crypto::ScopedMockUnexportableKeyProvider>();
    }

    base::test::TestFuture<bool> future;
    delegate.BrowserProvidedPasskeysAvailable(browser_context(),
                                              future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(future.Get(), test.expected_passkeys_available);
    signin::ClearPrimaryAccount(identity_manager);
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

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

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeAuthenticatorRequestDelegateTest {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kCorpCrdAutopushOrigin[] =
      "https://remotedesktop-autopush.corp.google.com/";
  static constexpr char kCorpCrdDailyOrigin[] =
      "https://remotedesktop-daily-6.corp.google.com/";
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
  for (auto* origin : {kCorpCrdOrigin, kCorpCrdAutopushOrigin,
                       kCorpCrdDailyOrigin, kExampleOrigin}) {
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

      constexpr const char* const crd_origins[] = {
          kCorpCrdOrigin,
          kCorpCrdAutopushOrigin,
          kCorpCrdDailyOrigin,
      };
      EXPECT_EQ(
          delegate.OriginMayUseRemoteDesktopClientOverride(
              browser_context(), url::Origin::Create(GURL(origin))),
          base::Contains(crd_origins, origin) && policy == Policy::kEnabled);
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

}  // namespace

#if BUILDFLAG(IS_MAC)

// These test functions are outside of the anonymous namespace so that
// `FRIEND_TEST_ALL_PREFIXES` works to let them test private functions.

class ChromeAuthenticatorRequestDelegatePrivateTest : public testing::Test {
  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, DaysSinceDate) {
  const base::Time now = base::Time::FromTimeT(1691188997);  // 2023-08-04
  const struct {
    char input[16];
    std::optional<int> expected_result;
  } kTestCases[] = {
      {"", std::nullopt},          //
      {"2023-08-", std::nullopt},  //
      {"2023-08-04", 0},           //
      {"2023-08-03", 1},           //
      {"2023-8-3", 1},             //
      {"2023-07-04", 31},          //
      {"2001-11-23", 7924},        //
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.input);
    const std::optional<int> result =
        ChromeAuthenticatorRequestDelegate::DaysSinceDate(test.input, now);
    EXPECT_EQ(result, test.expected_result);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, GetICloudKeychainPref) {
  TestingProfile profile;

  // We use a boolean preference as a tristate, so it's worth checking that
  // an unset preference is recognised as such.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                   profile.GetPrefs())
                   .has_value());
  profile.GetPrefs()->SetBoolean(prefs::kCreatePasskeysInICloudKeychain, true);
  EXPECT_EQ(*ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                profile.GetPrefs()),
            true);
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest,
       ShouldCreateInICloudKeychain) {
  // Safety check that SPC requests never default to iCloud Keychain.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
      ChromeAuthenticatorRequestDelegate::RequestSource::
          kSecurePaymentConfirmation,
      /*is_active_profile_authenticator_user=*/false,
      /*has_icloud_drive_enabled=*/true, /*request_is_for_google_com=*/true,
      /*preference=*/true));

  // For the valid request type, the preference should be controlling if set.
  for (const bool preference : {false, true}) {
    EXPECT_EQ(preference,
              ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
                  ChromeAuthenticatorRequestDelegate::RequestSource::
                      kWebAuthentication,
                  /*is_active_profile_authenticator_user=*/false,
                  /*has_icloud_drive_enabled=*/true,
                  /*request_is_for_google_com=*/true,
                  /*preference=*/preference));

    // Otherwise the default is controlled by several feature flags. Testing
    // them would just be a change detector.
  }
}

#endif
