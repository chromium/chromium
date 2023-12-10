// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace password_manager_util {
namespace {

using ::autofill::password_generation::PasswordGenerationType;
using ::device_reauth::MockDeviceAuthenticator;
using ::password_manager::Facet;
using ::password_manager::FacetURI;
using ::password_manager::GroupedFacets;
using ::password_manager::PasswordForm;

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestProxyOrigin[] = "http://proxy.com/";
constexpr char kTestProxySignonRealm[] = "proxy.com/realm";
constexpr char kTestURL[] = "https://example.com/login/";
constexpr char16_t kTestUsername[] = u"Username";
constexpr char16_t kTestPassword[] = u"12345";

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(
                   password_manager::PasswordManagerClient::ReauthSucceeded)>),
              (override));
  MOCK_METHOD(void, GeneratePassword, (PasswordGenerationType), (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(PrefService*, GetLocalStatePrefs, (), (const, override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
};

PasswordForm GetTestAndroidCredential() {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(kTestAndroidRealm);
  form.signon_realm = kTestAndroidRealm;
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

PasswordForm GetTestCredential() {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(kTestURL);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

PasswordForm GetTestProxyCredential() {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kBasic;
  form.url = GURL(kTestProxyOrigin);
  form.signon_realm = kTestProxySignonRealm;
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::Return;

class PasswordManagerUtilTest : public testing::Test {
 public:
  PasswordManagerUtilTest() {
    authenticator_ = std::make_unique<device_reauth::MockDeviceAuthenticator>();
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableAutosignin, true);
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kOfferToSavePasswordsEnabledGMS, true);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kSavePasswordsSuspendedByError, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kAutoSignInEnabledGMS, true);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        false);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kHadBiometricsAvailable, false);
    ON_CALL(mock_client_, GetLocalStatePrefs())
        .WillByDefault(Return(&pref_service_));
    ON_CALL(mock_client_, GetPrefs()).WillByDefault(Return(&pref_service_));
    ON_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
        .WillByDefault(Return(true));
#endif
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  void SetBiometricAuthenticationBeforeFilling(bool available) {
    pref_service_.SetBoolean(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling,
        available);
  }
#endif

 protected:
  MockPasswordManagerClient mock_client_;
  std::unique_ptr<device_reauth::MockDeviceAuthenticator> authenticator_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

TEST(PasswordManagerUtil, GetSignonRealmWithProtocolExcluded) {
  PasswordForm http_form;
  http_form.url = GURL("http://www.google.com/page-1/");
  http_form.signon_realm = "http://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(http_form), "www.google.com/");

  PasswordForm https_form;
  https_form.url = GURL("https://www.google.com/page-1/");
  https_form.signon_realm = "https://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(https_form), "www.google.com/");

  PasswordForm federated_form;
  federated_form.url = GURL("http://localhost:8000/");
  federated_form.signon_realm =
      "federation://localhost/accounts.federation.com";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(federated_form),
            "localhost/accounts.federation.com");
}

TEST(PasswordManagerUtil, GetMatchType_Android) {
  PasswordForm form = GetTestAndroidCredential();
  form.match_type = PasswordForm::MatchType::kAffiliated;

  EXPECT_EQ(GetLoginMatchType::kAffiliated, GetMatchType(form));
}

TEST(PasswordManagerUtil, GetMatchType_Web) {
  PasswordForm form = GetTestCredential();

  form.match_type = PasswordForm::MatchType::kExact;
  EXPECT_EQ(GetLoginMatchType::kExact, GetMatchType(form));

  form.match_type =
      PasswordForm::MatchType::kPSL | PasswordForm::MatchType::kAffiliated;
  EXPECT_EQ(GetLoginMatchType::kAffiliated, GetMatchType(form));

  form.match_type = PasswordForm::MatchType::kAffiliated;
  EXPECT_EQ(GetLoginMatchType::kAffiliated, GetMatchType(form));

  form.match_type = PasswordForm::MatchType::kPSL;
  EXPECT_EQ(GetLoginMatchType::kPSL, GetMatchType(form));
}

TEST(PasswordManagerUtil, GetMatchType_Grouped) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kFillingAcrossGroupedSites);

  PasswordForm form = GetTestAndroidCredential();
  form.match_type = PasswordForm::MatchType::kGrouped;
  EXPECT_EQ(GetLoginMatchType::kAffiliated, GetMatchType(form));
}

TEST(PasswordManagerUtil, FindBestMatches) {
  const base::Time kNow = base::Time::Now();
  const base::Time kYesterday = kNow - base::Days(1);
  const base::Time k2DaysAgo = kNow - base::Days(2);
  const int kNotFound = -1;
  struct TestMatch {
    PasswordForm::MatchType match_type;
    base::Time date_last_used;
    std::u16string username;
    std::string signon_realm = kTestURL;
  };
  struct TestCase {
    const char* description;
    std::vector<TestMatch> matches;
    int expected_preferred_match_index;
    std::map<std::string, size_t> expected_best_matches_indices;
  } test_cases[] = {
      {"Empty matches", {}, kNotFound, {}},
      {"1 exact match",
       {{.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kNow,
         .username = u"u"}},
       /*expected_preferred_match_index=*/0,
       {{"u", 0}}},
      {"1 psl match",
       {{.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kNow,
         .username = u"u"}},
       /*expected_preferred_match_index=*/0,
       {{"u", 0}}},
      {"2 matches with the same username",
       {{.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kNow,
         .username = u"u"},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kYesterday,
         .username = u"u"}},
       /*expected_preferred_match_index=*/0,
       {{"u", 0}}},
      {"2 matches with different usernames, most recently used taken",
       {{.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kNow,
         .username = u"u1"},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kYesterday,
         .username = u"u2"}},
       /*expected_preferred_match_index=*/0,
       {{"u1", 0}, {"u2", 1}}},
      {"2 matches with different usernames, exact match taken",
       {{.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kYesterday,
         .username = u"u1"},
        {.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kNow,
         .username = u"u2"}},
       /*expected_preferred_match_index=*/0,
       {{"u1", 0}, {"u2", 1}}},
      {"8 matches, 3 usernames",
       {{.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kYesterday,
         .username = u"u2"},
        {.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kYesterday,
         .username = u"u3"},
        {.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kYesterday,
         .username = u"u1"},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = k2DaysAgo,
         .username = u"u3"},
        {.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kNow,
         .username = u"u1"},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kNow,
         .username = u"u2"},
        {.match_type = PasswordForm::MatchType::kPSL,
         .date_last_used = kYesterday,
         .username = u"u3"},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = k2DaysAgo,
         .username = u"u1"}},
       /*expected_preferred_match_index=*/5,
       {{"u1", 7}, {"u2", 5}, {"u3", 3}}},
      {"Affiliated Android app and exact matches, exact match taken",
       {{.match_type = PasswordForm::MatchType::kAffiliated,
         .date_last_used = kNow,
         .username = u"uAndroid",
         .signon_realm = kTestAndroidRealm},
        {.match_type = PasswordForm::MatchType::kExact,
         .date_last_used = kYesterday,
         .username = u"uExact"}},
       /*expected_preferred_match_index=*/1,
       {{"uExact", 1}, {"uAndroid", 0}}},
      {"Affiliated Android app and affiliated website matches, most recently "
       "used taken",
       {{.match_type = PasswordForm::MatchType::kAffiliated,
         .date_last_used = kYesterday,
         .username = u"uAffiliatedAndroid",
         .signon_realm = kTestAndroidRealm},
        {.match_type = PasswordForm::MatchType::kAffiliated,
         .date_last_used = kNow,
         .username = u"uAffiliatedWebsite"}},
       /*expected_preferred_match_index=*/1,
       {{"uAffiliatedWebsite", 1}, {"uAffiliatedAndroid", 0}}},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description);
    // Convert TestMatch to PasswordForm.
    std::vector<PasswordForm> owning_matches;
    for (const TestMatch& match : test_case.matches) {
      PasswordForm form;
      form.match_type = match.match_type;
      form.signon_realm = match.signon_realm;
      form.date_last_used = match.date_last_used;
      form.username_value = match.username;
      owning_matches.push_back(form);
    }
    std::vector<const PasswordForm*> matches;
    for (const PasswordForm& match : owning_matches)
      matches.push_back(&match);

    std::vector<const PasswordForm*> best_matches;
    const PasswordForm* preferred_match = nullptr;

    std::vector<const PasswordForm*> same_scheme_matches;
    FindBestMatches(matches, PasswordForm::Scheme::kHtml, &same_scheme_matches,
                    &best_matches);
    if (!best_matches.empty()) {
      preferred_match = best_matches[0];
    }

    if (test_case.expected_preferred_match_index == kNotFound) {
      // Case of empty |matches|.
      EXPECT_FALSE(preferred_match);
      EXPECT_TRUE(best_matches.empty());
    } else {
      // Check |preferred_match|.
      EXPECT_EQ(matches[test_case.expected_preferred_match_index],
                preferred_match);
      // Check best matches.
      ASSERT_EQ(test_case.expected_best_matches_indices.size(),
                best_matches.size());

      for (const PasswordForm* match : best_matches) {
        std::string username = base::UTF16ToUTF8(match->username_value);
        ASSERT_NE(test_case.expected_best_matches_indices.end(),
                  test_case.expected_best_matches_indices.find(username));
        size_t expected_index =
            test_case.expected_best_matches_indices.at(username);
        size_t actual_index =
            std::distance(matches.begin(), base::ranges::find(matches, match));
        EXPECT_EQ(expected_index, actual_index);
      }
    }
  }
}

TEST(PasswordManagerUtil, FindBestMatchesInProfileAndAccountStores) {
  const std::u16string kUsername1 = u"Username1";
  const std::u16string kPassword1 = u"Password1";
  const std::u16string kUsername2 = u"Username2";
  const std::u16string kPassword2 = u"Password2";

  PasswordForm form;
  form.match_type = PasswordForm::MatchType::kExact;
  form.date_last_used = base::Time::Now();

  // Add the same credentials in account and profile stores.
  PasswordForm account_form1(form);
  account_form1.username_value = kUsername1;
  account_form1.password_value = kPassword1;
  account_form1.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_form1(account_form1);
  profile_form1.in_store = PasswordForm::Store::kProfileStore;

  // Add the credentials for the same username in account and profile stores but
  // with different passwords.
  PasswordForm account_form2(form);
  account_form2.username_value = kUsername2;
  account_form2.password_value = kPassword1;
  account_form2.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_form2(account_form2);
  profile_form2.password_value = kPassword2;
  profile_form2.in_store = PasswordForm::Store::kProfileStore;

  std::vector<const PasswordForm*> matches;
  matches.push_back(&account_form1);
  matches.push_back(&profile_form1);
  matches.push_back(&account_form2);
  matches.push_back(&profile_form2);

  std::vector<const PasswordForm*> best_matches;
  std::vector<const PasswordForm*> same_scheme_matches;
  FindBestMatches(matches, PasswordForm::Scheme::kHtml, &same_scheme_matches,
                  &best_matches);
  // |profile_form1| is filtered out because it's the same as |account_form1|.
  EXPECT_EQ(best_matches.size(), 3U);
  EXPECT_TRUE(base::Contains(best_matches, &account_form1));
  EXPECT_TRUE(base::Contains(best_matches, &account_form2));
  EXPECT_FALSE(base::Contains(best_matches, &profile_form1));
  EXPECT_TRUE(base::Contains(best_matches, &profile_form2));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsername) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kExact;
  PasswordForm parsed = GetTestCredential();
  parsed.password_value = u"new_password";

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_RejectUnknownUsername) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kExact;
  PasswordForm parsed = GetTestCredential();
  parsed.username_value = u"other_username";

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_FederatedCredential) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kExact;
  PasswordForm parsed = GetTestCredential();
  parsed.password_value.clear();
  parsed.federation_origin = url::Origin::Create(GURL(kTestFederationURL));

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSL) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kPSL;
  PasswordForm parsed = GetTestCredential();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSLAnotherPassword) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kPSL;
  PasswordForm parsed = GetTestCredential();
  parsed.password_value = u"new_password";

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordKnown) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kPSL;
  PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = parsed.password_value;
  parsed.password_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordUnknown) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kPSL;
  PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = u"new_password";
  parsed.password_value.clear();

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPassword) {
  PasswordForm stored = GetTestCredential();
  PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPasswordPSL) {
  PasswordForm stored = GetTestCredential();
  stored.match_type = PasswordForm::MatchType::kPSL;
  PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameCMAPI) {
  PasswordForm stored = GetTestCredential();
  PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();
  parsed.type = PasswordForm::Type::kApi;

  // In case of the Credential Management API we know for sure that the site
  // meant empty username. Don't try any other heuristics.
  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernamePickFirst) {
  PasswordForm stored1 = GetTestCredential();
  stored1.username_value = u"Adam";
  stored1.password_value = u"Adam_password";
  PasswordForm stored2 = GetTestCredential();
  stored2.username_value = u"Ben";
  stored2.password_value = u"Ben_password";
  PasswordForm stored3 = GetTestCredential();
  stored3.username_value = u"Cindy";
  stored3.password_value = u"Cindy_password";

  PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  // The first credential is picked (arbitrarily).
  EXPECT_EQ(&stored3,
            GetMatchForUpdating(parsed, {&stored3, &stored2, &stored1}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_EmptyUsernameManualInputNewPassword) {
  PasswordForm stored = GetTestCredential();
  stored.username_value = u"Adam";
  stored.password_value = u"Adam_password";

  PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(nullptr,
            GetMatchForUpdating(parsed, {&stored},
                                /* username_updated_in_bubble */ true));
}

TEST(PasswordManagerUtil, MakeNormalizedBlocklistedForm_Android) {
  PasswordForm blocklisted_credential = MakeNormalizedBlocklistedForm(
      password_manager::PasswordFormDigest(GetTestAndroidCredential()));
  EXPECT_TRUE(blocklisted_credential.blocked_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blocklisted_credential.scheme);
  EXPECT_EQ(kTestAndroidRealm, blocklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestAndroidRealm), blocklisted_credential.url);
}

TEST(PasswordManagerUtil, MakeNormalizedBlocklistedForm_Html) {
  PasswordForm blocklisted_credential = MakeNormalizedBlocklistedForm(
      password_manager::PasswordFormDigest(GetTestCredential()));
  EXPECT_TRUE(blocklisted_credential.blocked_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blocklisted_credential.scheme);
  EXPECT_EQ(GURL(kTestURL).DeprecatedGetOriginAsURL().spec(),
            blocklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestURL).DeprecatedGetOriginAsURL(),
            blocklisted_credential.url);
}

TEST(PasswordManagerUtil, MakeNormalizedBlocklistedForm_Proxy) {
  PasswordForm blocklisted_credential = MakeNormalizedBlocklistedForm(
      password_manager::PasswordFormDigest(GetTestProxyCredential()));
  EXPECT_TRUE(blocklisted_credential.blocked_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kBasic, blocklisted_credential.scheme);
  EXPECT_EQ(kTestProxySignonRealm, blocklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestProxyOrigin), blocklisted_credential.url);
}

TEST(PasswordManagerUtil, ManualGenerationShouldNotReauthIfNotNeeded) {
  MockPasswordManagerClient mock_client;
  ON_CALL(*(mock_client.GetPasswordFeatureManager()),
          ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(false));

  EXPECT_CALL(mock_client, TriggerReauthForPrimaryAccount).Times(0);
  EXPECT_CALL(mock_client, GeneratePassword(PasswordGenerationType::kManual));

  UserTriggeredManualGenerationFromContextMenu(&mock_client, nullptr);
}

TEST(PasswordManagerUtil,
     ManualGenerationShouldGeneratePasswordIfReauthSucessful) {
  MockPasswordManagerClient mock_client;
  ON_CALL(*(mock_client.GetPasswordFeatureManager()),
          ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));

  EXPECT_CALL(
      mock_client,
      TriggerReauthForPrimaryAccount(
          signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu, _))
      .WillOnce(
          [](signin_metrics::ReauthAccessPoint,
             base::OnceCallback<void(
                 password_manager::PasswordManagerClient::ReauthSucceeded)>
                 callback) {
            std::move(callback).Run(
                password_manager::PasswordManagerClient::ReauthSucceeded(true));
          });
  EXPECT_CALL(mock_client, GeneratePassword(PasswordGenerationType::kManual));

  UserTriggeredManualGenerationFromContextMenu(&mock_client, nullptr);
}

TEST(PasswordManagerUtil,
     ManualGenerationShouldNotGeneratePasswordIfReauthFailed) {
  MockPasswordManagerClient mock_client;
  ON_CALL(*(mock_client.GetPasswordFeatureManager()),
          ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));

  EXPECT_CALL(
      mock_client,
      TriggerReauthForPrimaryAccount(
          signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu, _))
      .WillOnce(
          [](signin_metrics::ReauthAccessPoint,
             base::OnceCallback<void(
                 password_manager::PasswordManagerClient::ReauthSucceeded)>
                 callback) {
            std::move(callback).Run(
                password_manager::PasswordManagerClient::ReauthSucceeded(
                    false));
          });
  EXPECT_CALL(mock_client, GeneratePassword).Times(0);

  UserTriggeredManualGenerationFromContextMenu(&mock_client, nullptr);
}

TEST(PasswordManagerUtil, AvoidOverlappingAutofillMenuAndManualGeneration) {
  base::test::TaskEnvironment task_environment;
  password_manager::StubPasswordManagerClient stub_password_client;
  autofill::TestAutofillClient test_autofill_client;

  test_autofill_client.ShowAutofillPopup(
      autofill::AutofillClient::PopupOpenArgs(), /*delegate=*/nullptr);
  UserTriggeredManualGenerationFromContextMenu(&stub_password_client,
                                               &test_autofill_client);
  EXPECT_EQ(
      test_autofill_client.popup_hiding_reason(),
      autofill::PopupHidingReason::kOverlappingWithPasswordGenerationPopup);
}

TEST(PasswordManagerUtil, StripAuthAndParams) {
  GURL url = GURL("https://login:password@example.com/login/?param=value#ref");
  EXPECT_EQ(GURL("https://example.com/login/"), StripAuthAndParams(url));
}

TEST(PasswordManagerUtil, ConstructGURLWithScheme) {
  std::vector<std::pair<std::string, GURL>> test_cases = {
      {"example.com", GURL("https://example.com")},
      {"127.0.0.1", GURL("http://127.0.0.1")},
      {"file:///Test/example.html", GURL("file:///Test/example.html")},
      {"https://www.example.com", GURL("https://www.example.com")},
      {"example", GURL("https://example")}};
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.second, ConstructGURLWithScheme(test_case.first));
  }
}

TEST(PasswordManagerUtil, GetSignonRealm) {
  std::vector<std::pair<GURL, std::string>> test_cases = {
      {GURL("http://example.com/"), "http://example.com/"},
      {GURL("http://example.com/signup"), "http://example.com/"},
      {GURL("https://google.com/auth?a=1#b"), "https://google.com/"},
      {GURL("https://username:password@google.com/"), "https://google.com/"}};
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.second, GetSignonRealm(test_case.first));
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(PasswordManagerUtilTest, CanUseBiometricAuth) {
  EXPECT_CALL(*(mock_client_.GetPasswordFeatureManager()),
              IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(Return(false));
  EXPECT_FALSE(CanUseBiometricAuth(authenticator_.get(), &mock_client_));

  EXPECT_CALL(*(mock_client_.GetPasswordFeatureManager()),
              IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(Return(true));
  EXPECT_TRUE(CanUseBiometricAuth(authenticator_.get(), &mock_client_));
}

TEST_F(PasswordManagerUtilTest, BiometricsUnavailable) {
  SetBiometricAuthenticationBeforeFilling(/*available=*/false);
  EXPECT_CALL(*authenticator_, CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator_))));
  EXPECT_FALSE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

TEST_F(PasswordManagerUtilTest, ShouldShowBiometricAuthPromo) {
  SetBiometricAuthenticationBeforeFilling(/*available=*/false);
  EXPECT_CALL(*authenticator_, CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator_))));
  EXPECT_TRUE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

#elif BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerUtilTest, CanUseBiometricAuthAndroidAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(CanUseBiometricAuth(authenticator_.get(), &mock_client_));
}

#endif

}  // namespace password_manager_util
