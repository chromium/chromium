// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/chrome_web_authentication_delegate.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/features.h"
#include "device/fido/fido_request_handler_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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

class ChromeWebAuthenticationDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
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
};

TEST_F(ChromeWebAuthenticationDelegateTest, IndividualAttestation) {
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
TEST_F(ChromeWebAuthenticationDelegateTest,
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
TEST_F(ChromeWebAuthenticationDelegateTest,
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
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionNotFound) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
      kExtensionId));
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// web origins.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_WebOrigin) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL("https://google.com")),
      kExtensionId));
}

TEST_F(ChromeWebAuthenticationDelegateTest, MaybeGetRelyingPartyIdOverride) {
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

TEST_F(ChromeWebAuthenticationDelegateTest, DeletePasskey) {
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

TEST_F(ChromeWebAuthenticationDelegateTest, DeleteUnacceptedPasskey) {
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

TEST_F(ChromeWebAuthenticationDelegateTest, UpdatePasskey) {
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

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeWebAuthenticationDelegateTest {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kCorpCrdAutopushOrigin[] =
      "https://remotedesktop-autopush.corp.google.com/";
  static constexpr char kCorpCrdDailyOrigin[] =
      "https://remotedesktop-daily-6.corp.google.com/";

  const std::array<const char*, 3> kCorpCrdOrigins = {
      kCorpCrdOrigin, kCorpCrdAutopushOrigin, kCorpCrdDailyOrigin};

  static constexpr char kExampleOrigin[] = "https://example.com";
  static constexpr char kAnotherExampleOrigin[] = "https://another.example.com";

  base::test::ScopedFeatureList scoped_feature_list_;
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
  for (auto* origin : kCorpCrdOrigins) {
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
       AdditionalOriginSwitch_WithGooglePolicy) {
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

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithAllowedOriginsPolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing. This origin will be
  // allowed if the kWebAuthnRemoteDesktopAllowedOriginsPolicy preference is set
  // to a non-empty list of origins.  If the policy is set, the command-line
  // origin is treated as another allowed origin in addition to those specified
  // by the policy.
  ChromeWebAuthenticationDelegate delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);
  scoped_feature_list_.InitAndEnableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);

  // Initially, no origins should be allowed because the allowed origins pref
  // hasn't been set yet.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Set the allowed origins pref to include another origin.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append(kAnotherExampleOrigin));

  // Both the origin specified by the command-line switch and the origin in the
  // allowed origins pref should be allowed.
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Google Corp CRD origins are not affected by either the switch or this
  // policy.
  for (auto* origin : kCorpCrdOrigins) {
    EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
        browser_context(), url::Origin::Create(GURL(origin))));
  }

  // Origins not listed in either the switch or the policy remain disallowed.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://very.other.example.com"))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithExplicitlyEmptyAllowedOriginsPolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // should be ignored when the allowed origins policy list is empty.
  ChromeWebAuthenticationDelegate delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);
  scoped_feature_list_.InitAndEnableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Test with policy unset.
  prefs->ClearPref(webauthn::pref_names::kRemoteDesktopAllowedOrigins);
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));

  // Test with policy explicitly empty.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List());
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_InvalidURLs) {
  ChromeWebAuthenticationDelegate delegate;
  scoped_feature_list_.InitAndEnableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  const std::vector<std::string> invalid_origins = {
      "invalid",
      "http://",
      "example.com",  // Missing scheme
      "https://example.com:invalidport",
  };

  base::Value::List invalid_origins_list;
  for (const auto& origin : invalid_origins) {
    invalid_origins_list.Append(origin);
  }
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 std::move(invalid_origins_list));

  // None of the above invalid origins should grant access.
  for (const auto& origin : invalid_origins) {
    EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
        browser_context(), url::Origin::Create(GURL(origin))));
  }

  // A valid one, added for good measure, should still work.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append(kExampleOrigin));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_FeatureDisabled) {
  ChromeWebAuthenticationDelegate delegate;
  // Feature explicitly disabled.
  scoped_feature_list_.InitAndDisableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append(kExampleOrigin));

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_MultipleValidURLs) {
  ChromeWebAuthenticationDelegate delegate;
  scoped_feature_list_.InitAndEnableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  base::Value::List valid_origins;
  valid_origins.Append(kExampleOrigin);
  valid_origins.Append(kAnotherExampleOrigin);
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 std::move(valid_origins));

  // Both origins specified in the policy should grant access.
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // An unrelated origin should not be allowed.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://very.other.example.com"))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_SchemePortPathMismatch) {
  ChromeWebAuthenticationDelegate delegate;
  scoped_feature_list_.InitAndEnableFeature(
      device::kWebAuthnRemoteDesktopAllowedOriginsPolicy);
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Scheme mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append("https://example.com"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("http://example.com"))));

  // Port mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append("https://example.com:1234"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com:5678"))));

  // Path mismatch (should be allowed because paths are ignored).
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::Value::List().Append("https://example.com/path"));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com/otherpath"))));
}

}  // namespace
