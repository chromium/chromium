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
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
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
    scoped_feature_list_.InitAndDisableFeature(
        device::kWebAuthnSignalApiHidePasskeys);
    PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);
  }

  void TearDown() override {
    webauthn::PasskeyChangeQuotaTracker::GetInstance()->ResetForTesting();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  Observer observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  const auto test_origin = url::Origin::Create(GURL("https://example.com"));
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
    delegate.PasskeyUnrecognized(web_contents(), test_origin,
                                 ToByteVector(kCredentialId2), kRpId);
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
    delegate.PasskeyUnrecognized(web_contents(), test_origin,
                                 ToByteVector(kCredentialId1), kRpId);
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
  const auto test_origin = url::Origin::Create(GURL("https://example.com"));
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
    delegate.SignalAllAcceptedCredentials(web_contents(), test_origin, kRpId,
                                          ToByteVector(kUserId),
                                          {ToByteVector(kCredentialId1)});
    EXPECT_TRUE(passkey_model->GetPasskeyByCredentialId(kRpId, kCredentialId1));
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
        ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
            kNoPasskeyChanged,
        1);
  }
  {
    // Do not pass the known credential. The known credential should be removed.
    base::HistogramTester histogram_tester;
    delegate.SignalAllAcceptedCredentials(web_contents(), test_origin, kRpId,
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

class ChromeWebAuthenticationSignalApiHidePasskeysTest
    : public ChromeWebAuthenticationDelegateTest {
 public:
  void SetUp() override {
    ChromeWebAuthenticationDelegateTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        device::kWebAuthnSignalApiHidePasskeys, true);
    passkey_model_ = PasskeyModelFactory::GetForProfile(profile());
    ASSERT_TRUE(passkey_model_);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    passkey_model_ = nullptr;
    ChromeWebAuthenticationDelegateTest::TearDown();
  }

  void AddPasskey(const std::string& credential_id) {
    sync_pb::WebauthnCredentialSpecifics passkey;
    passkey.set_credential_id(credential_id);
    passkey.set_rp_id(kRpId);
    passkey.set_user_id(kUserId);
    passkey_model_->AddNewPasskeyForTesting(std::move(passkey));
  }

  void AddHiddenPasskey(const std::string& credential_id) {
    sync_pb::WebauthnCredentialSpecifics passkey;
    passkey.set_credential_id(credential_id);
    passkey.set_rp_id(kRpId);
    passkey.set_user_id(kUserId);
    passkey.set_hidden(true);
    passkey_model_->AddNewPasskeyForTesting(std::move(passkey));
  }

 protected:
  sync_pb::WebauthnCredentialSpecifics GetPasskey(const std::string& cred_id) {
    return *passkey_model_->GetPasskeyByCredentialId(kRpId, cred_id);
  }

  const url::Origin test_origin_ =
      url::Origin::Create(GURL("https://example.com"));
  ChromeWebAuthenticationDelegate delegate_;
  raw_ptr<webauthn::PasskeyModel> passkey_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest, Unrecognized_Found) {
  AddPasskey(kCredentialId1);
  ASSERT_FALSE(GetPasskey(kCredentialId1).hidden());
  delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                ToByteVector(kCredentialId1), kRpId);
  EXPECT_TRUE(GetPasskey(kCredentialId1).hidden());

  histogram_tester_->ExpectUniqueSample(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
          kPasskeyHidden,
      1);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       Unrecognized_AlreadyHidden) {
  AddPasskey(kCredentialId1);
  passkey_model_->SetPasskeyHidden(kCredentialId1, true);
  delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                ToByteVector(kCredentialId1), kRpId);
  EXPECT_TRUE(GetPasskey(kCredentialId1).hidden());

  histogram_tester_->ExpectUniqueSample(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
          kPasskeyAlreadyHidden,
      1);

  // Check that the quota does not apply if no change happens.
  for (int i = 0; i < webauthn::PasskeyChangeQuotaTracker::kMaxTokensPerRP;
       ++i) {
    delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                  ToByteVector(kCredentialId1), kRpId);
  }
  passkey_model_->SetPasskeyHidden(kCredentialId1, false);
  delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                ToByteVector(kCredentialId1), kRpId);
  EXPECT_TRUE(GetPasskey(kCredentialId1).hidden());
  histogram_tester_->ExpectBucketCount(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
          kQuotaExceeded,
      0);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       Unrecognized_NotFound) {
  delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                ToByteVector(kCredentialId1), kRpId);
  histogram_tester_->ExpectUniqueSample(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
          kPasskeyNotFound,
      1);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       Unrecognized_QuotaExceeded) {
  AddPasskey(kCredentialId1);
  for (int i = 0; i < webauthn::PasskeyChangeQuotaTracker::kMaxTokensPerRP;
       ++i) {
    delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                  ToByteVector(kCredentialId1), kRpId);
    passkey_model_->SetPasskeyHidden(kCredentialId1, false);
  }
  base::HistogramTester histogram_tester;
  delegate_.PasskeyUnrecognized(web_contents(), test_origin_,
                                ToByteVector(kCredentialId1), kRpId);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalUnknownCredentialResult::
          kQuotaExceeded,
      1);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_Hide) {
  base::HistogramTester histogram_tester;
  AddPasskey(kCredentialId1);

  // Pass a list that does not contain the hidden passkey.
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId2)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                         ToByteVector(kUserId), credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kPasskeyHidden,
      1);
  // The originally active passkey should be hidden.
  EXPECT_TRUE(GetPasskey(kCredentialId1).hidden());
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_Restore) {
  base::HistogramTester histogram_tester;
  AddHiddenPasskey(kCredentialId1);

  // Pass a list that contains the hidden passkey.
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId1)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                         ToByteVector(kUserId), credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kPasskeyRestored,
      1);
  // The passkey should have been restored.
  EXPECT_FALSE(GetPasskey(kCredentialId1).hidden());
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_NoChanges) {
  base::HistogramTester histogram_tester;
  AddPasskey(kCredentialId1);

  // Pass a list that contains the active passkey.
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId1)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                         ToByteVector(kUserId), credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kNoPasskeyChanged,
      1);
  // The passkey should still be visible.
  EXPECT_FALSE(GetPasskey(kCredentialId1).hidden());
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_NoPasskeysMatch_RpId) {
  base::HistogramTester histogram_tester;
  AddPasskey(kCredentialId1);

  // Pass a list that contains passkeys from a different relying party.
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId1)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_,
                                         "another.com", ToByteVector(kUserId),
                                         credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kNoPasskeyChanged,
      1);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_NoPasskeysMatch_UserId) {
  base::HistogramTester histogram_tester;
  AddPasskey(kCredentialId1);

  // Pass a list that contains passkeys from a different user id.
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId1)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                         ToByteVector("another-userid"),
                                         credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kNoPasskeyChanged,
      1);
}

TEST_F(ChromeWebAuthenticationSignalApiHidePasskeysTest,
       SignalAllAcceptedCredentials_QuotaExceeded) {
  AddPasskey(kCredentialId1);

  // Exceed the quota.
  for (int i = 0; i < webauthn::PasskeyChangeQuotaTracker::kMaxTokensPerRP;
       ++i) {
    std::vector<std::vector<uint8_t>> credentials = {
        ToByteVector(i % 2 == 0 ? kCredentialId2 : kCredentialId1)};
    delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                           ToByteVector(kUserId), credentials);
  }

  // Attempt making another change that would hide the passkey.
  passkey_model_->SetPasskeyHidden(kCredentialId1, false);
  base::HistogramTester histogram_tester;
  std::vector<std::vector<uint8_t>> credentials = {
      ToByteVector(kCredentialId2)};
  delegate_.SignalAllAcceptedCredentials(web_contents(), test_origin_, kRpId,
                                         ToByteVector(kUserId), credentials);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      ChromeWebAuthenticationDelegate::SignalAllAcceptedCredentialsResult::
          kQuotaExceeded,
      1);
  EXPECT_FALSE(GetPasskey(kCredentialId1).hidden());
}

}  // namespace
