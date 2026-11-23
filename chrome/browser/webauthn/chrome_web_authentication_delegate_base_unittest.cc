// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate_base.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "components/webauthn/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kExampleOrigin[] = "https://example.com";
#if !BUILDFLAG(IS_ANDROID)
constexpr char kTestIsolatedAppOrigin[] =
    "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
#endif

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kCorpCrdAutopushOrigin[] =
      "https://remotedesktop-autopush.corp.google.com/";
  static constexpr char kCorpCrdStagingOrigin[] =
      "https://remotedesktop-staging.corp.google.com/";
  // TODO(crbug.com/559634151): Remove daily-6 after CRD staging transition is
  // complete.
  static constexpr char kCorpCrdDailyOrigin[] =
      "https://remotedesktop-daily-6.corp.google.com/";

  const std::array<const char*, 4> kCorpCrdOrigins = {
      kCorpCrdOrigin, kCorpCrdAutopushOrigin, kCorpCrdStagingOrigin,
      kCorpCrdDailyOrigin};

  static constexpr char kAnotherExampleOrigin[] = "https://another.example.com";
  static constexpr std::string_view kTestAtExampleDotCom = "test@example.com";

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetupUserAffiliation(true, kTestAtExampleDotCom);
  }

  void SetupUserAffiliation(bool is_affiliated, std::string_view email) {
    const base::flat_set<std::string> affiliation_ids =
        is_affiliated ? base::flat_set<std::string>({"test-affiliation-id"})
                      : base::flat_set<std::string>();

    g_browser_process->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting(affiliation_ids);

    profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
        affiliation_ids);
  }

  void LogOutUser(std::string_view email) {
    g_browser_process->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting({});
    profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting({});
  }
};

#if !BUILDFLAG(IS_ANDROID)
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
  ChromeWebAuthenticationDelegateBase delegate;
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
          kCorpCrdStagingOrigin,
          // TODO(crbug.com/559634151): Remove daily-6 after CRD staging
          // transition is complete.
          kCorpCrdDailyOrigin,
      };
      EXPECT_EQ(delegate.OriginMayUseRemoteDesktopClientOverride(
                    browser_context(), url::Origin::Create(GURL(origin))),
                std::ranges::contains(crd_origins, origin) &&
                    policy == Policy::kEnabled);
    }
  }
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithGooglePolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing.
  ChromeWebAuthenticationDelegateBase delegate;
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
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithAllowedOriginsPolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing. This origin will be
  // allowed if the WebAuthenticationRemoteDesktopAllowedOrigins policy is set
  // to a non-empty list of origins.  If the policy is set, the command-line
  // origin is treated as another allowed origin in addition to those specified
  // by the policy.
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

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
                 base::ListValue().Append(kAnotherExampleOrigin));

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
       AdditionalOriginSwitch_WithAllowedOriginsPolicy_NoAffiliation) {
  // Version of a test case above but for non-affiliated user
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  // Log user out, no affiliated user exists for this test
  LogOutUser(kTestAtExampleDotCom);

  // Set the allowed origins pref to include another origin.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kAnotherExampleOrigin));

  // False when user is not affiliated or no user regardless if policy set with
  // origin
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Behavior for case without affiliated user in a sense of corporate origins
  // should not change
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
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Test with policy unset.
  prefs->ClearPref(webauthn::pref_names::kRemoteDesktopAllowedOrigins);
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));

  // Test with policy explicitly empty.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue());
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

#if !BUILDFLAG(IS_ANDROID)  // IWAs aren't supported on Android
TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWAAccepted) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kTestIsolatedAppOrigin));

  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWANotAccepted_Another_Caller_Origin) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kExampleOrigin));

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWAsNotAccepted_Origin_Not_Listed) {
  ChromeWebAuthenticationDelegateBase delegate;

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_InvalidURLs) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  const std::vector<std::string> invalid_origins = {
      "invalid",
      "http://",
      "example.com",  // Missing scheme
      "https://example.com:invalidport",
  };

  base::ListValue invalid_origins_list;
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
                 base::ListValue().Append(kExampleOrigin));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_MultipleValidURLs) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  base::ListValue valid_origins;
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
  ChromeWebAuthenticationDelegateBase delegate;
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Scheme mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("http://example.com"))));

  // Port mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com:1234"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com:5678"))));

  // Path mismatch (should be allowed because paths are ignored).
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com/path"));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com/otherpath"))));
}

#if BUILDFLAG(IS_CHROMEOS)
// Runs each test in a session of a device-local account of the given type.
// Device-local accounts have no user affiliation IDs, so the generic
// enterprise affiliation check reports them as unaffiliated.
class OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest(
      user_manager::UserType user_type,
      policy::DeviceLocalAccountType account_type)
      : ChromeRenderViewHostTestHarness(LogInParams{
            .account_id = AccountId::FromUserEmail(
                policy::GenerateDeviceLocalAccountUserId("dla", account_type)),
            .user_type = user_type}) {}

  void SetAllowedOriginsPolicy(const char* origin) {
    profile()->GetPrefs()->SetList(
        webauthn::pref_names::kRemoteDesktopAllowedOrigins,
        base::ListValue().Append(origin));
  }
};

// Kiosk sessions are always affiliated, so the allowed origins policy applies
// to them.
class OriginMayUseRemoteDesktopClientOverrideKioskTest
    : public OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest {
 public:
  OriginMayUseRemoteDesktopClientOverrideKioskTest()
      : OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest(
            user_manager::UserType::kKioskIWA,
            policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {}
};

TEST_F(OriginMayUseRemoteDesktopClientOverrideKioskTest,
       AllowedOriginsPolicy_IWAAcceptedWithoutAffiliationIds) {
  ASSERT_FALSE(enterprise_util::IsProfileAffiliated(profile()));

  ChromeWebAuthenticationDelegateBase delegate;
  SetAllowedOriginsPolicy(kTestIsolatedAppOrigin);

  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
  // Origins that are not listed by the policy are still rejected.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideKioskTest,
       NoAllowedOriginsPolicy_IWANotAccepted) {
  ChromeWebAuthenticationDelegateBase delegate;

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideKioskTest,
       KillSwitchDisabled_IWANotAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      webauthn::features::kWebAuthnRemoteDesktopClientOverrideForKiosk);

  ChromeWebAuthenticationDelegateBase delegate;
  SetAllowedOriginsPolicy(kTestIsolatedAppOrigin);

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

// Managed guest sessions are not covered by the kiosk exception and keep using
// the generic affiliation check, so the policy does not apply to them.
class OriginMayUseRemoteDesktopClientOverrideManagedGuestSessionTest
    : public OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest {
 public:
  OriginMayUseRemoteDesktopClientOverrideManagedGuestSessionTest()
      : OriginMayUseRemoteDesktopClientOverrideDeviceLocalAccountTest(
            user_manager::UserType::kPublicAccount,
            policy::DeviceLocalAccountType::kPublicSession) {}
};

TEST_F(OriginMayUseRemoteDesktopClientOverrideManagedGuestSessionTest,
       AllowedOriginsPolicy_IWANotAccepted) {
  ASSERT_FALSE(enterprise_util::IsProfileAffiliated(profile()));

  ChromeWebAuthenticationDelegateBase delegate;
  SetAllowedOriginsPolicy(kTestIsolatedAppOrigin);

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
