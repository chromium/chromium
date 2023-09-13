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
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
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
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::password_generation::PasswordGenerationType;
using device_reauth::MockDeviceAuthenticator;
using password_manager::Facet;
using password_manager::FacetURI;
using password_manager::GroupedFacets;
using password_manager::PasswordForm;

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestProxyOrigin[] = "http://proxy.com/";
constexpr char kTestProxySignonRealm[] = "proxy.com/realm";
constexpr char kTestURL[] = "https://example.com/login/";
constexpr char16_t kTestUsername[] = u"Username";
constexpr char16_t kTestUsername2[] = u"Username2";
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
  MOCK_METHOD(scoped_refptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
};

class MockAutofillClient : public autofill::AutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(version_info::Channel, GetChannel, (), (const, override));
  MOCK_METHOD(bool, IsOffTheRecord, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (override));
  MOCK_METHOD(autofill::AutocompleteHistoryManager*,
              GetAutocompleteHistoryManager,
              (),
              (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (override));
  MOCK_METHOD(const PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(syncer::SyncService*, GetSyncService, (), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(autofill::FormDataImporter*, GetFormDataImporter, (), (override));
  MOCK_METHOD(autofill::payments::PaymentsClient*,
              GetPaymentsClient,
              (),
              (override));
  MOCK_METHOD(autofill::StrikeDatabase*, GetStrikeDatabase, (), (override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, (), (override));
  MOCK_METHOD(ukm::SourceId, GetUkmSourceId, (), (override));
  MOCK_METHOD(autofill::AddressNormalizer*,
              GetAddressNormalizer,
              (),
              (override));
  MOCK_METHOD(const GURL&,
              GetLastCommittedPrimaryMainFrameURL,
              (),
              (const, override));
  MOCK_METHOD(url::Origin,
              GetLastCommittedPrimaryMainFrameOrigin,
              (),
              (const, override));
  MOCK_METHOD(security_state::SecurityLevel,
              GetSecurityLevelForUmaHistograms,
              (),
              (override));
  MOCK_METHOD(const translate::LanguageState*,
              GetLanguageState,
              (),
              (override));
  MOCK_METHOD(translate::TranslateDriver*, GetTranslateDriver, (), (override));
  MOCK_METHOD(void, ShowAutofillSettings, (autofill::PopupType), (override));
  MOCK_METHOD(void,
              ShowUnmaskPrompt,
              (const autofill::CreditCard&,
               const autofill::CardUnmaskPromptOptions&,
               base::WeakPtr<autofill::CardUnmaskDelegate>),
              (override));
  MOCK_METHOD(void,
              OnUnmaskVerificationResult,
              (PaymentsRpcResult),
              (override));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD(std::vector<std::string>,
              GetAllowedMerchantsForVirtualCards,
              (),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAllowedBinRangesForVirtualCards,
              (),
              (override));
  MOCK_METHOD(void,
              ShowLocalCardMigrationDialog,
              (base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              ConfirmMigrateLocalCardToCloud,
              (const autofill::LegalMessageLines&,
               const std::string&,
               const std::vector<autofill::MigratableCreditCard>&,
               LocalCardMigrationCallback),
              (override));
  MOCK_METHOD(void,
              ShowLocalCardMigrationResults,
              (const bool,
               const std::u16string&,
               const std::vector<autofill::MigratableCreditCard>&,
               MigrationDeleteCardCallback),
              (override));
  MOCK_METHOD(void,
              ConfirmSaveIbanLocally,
              (const autofill::Iban&, bool, LocalSaveIbanPromptCallback),
              (override));
  MOCK_METHOD(void,
              ShowWebauthnOfferDialog,
              (WebauthnDialogCallback),
              (override));
  MOCK_METHOD(void,
              ShowWebauthnVerifyPendingDialog,
              (WebauthnDialogCallback),
              (override));
  MOCK_METHOD(void, UpdateWebauthnOfferDialogWithError, (), (override));
  MOCK_METHOD(bool, CloseWebauthnDialog, (), (override));
  MOCK_METHOD(void,
              OfferVirtualCardOptions,
              (const std::vector<autofill::CreditCard*>&,
               base::OnceCallback<void(const std::string&)>),
              (override));
#else  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  MOCK_METHOD(void,
              ConfirmAccountNameFixFlow,
              (base::OnceCallback<void(const std::u16string&)>),
              (override));
  MOCK_METHOD(
      void,
      ConfirmExpirationDateFixFlow,
      (const autofill::CreditCard&,
       base::OnceCallback<void(const std::u16string&, const std::u16string&)>),
      (override));
#endif
  MOCK_METHOD(void,
              ConfirmSaveCreditCardLocally,
              (const autofill::CreditCard&,
               autofill::AutofillClient::SaveCreditCardOptions,
               LocalSaveCardPromptCallback),
              (override));
  MOCK_METHOD(void,
              ConfirmSaveCreditCardToCloud,
              (const autofill::CreditCard&,
               const autofill::LegalMessageLines&,
               SaveCreditCardOptions,
               UploadSaveCardPromptCallback),
              (override));
  MOCK_METHOD(void, CreditCardUploadCompleted, (bool), (override));
  MOCK_METHOD(void,
              ConfirmCreditCardFillAssist,
              (const autofill::CreditCard&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              ConfirmSaveAddressProfile,
              (const autofill::AutofillProfile&,
               const autofill::AutofillProfile*,
               SaveAddressProfilePromptOptions,
               AddressProfileSavePromptCallback),
              (override));
  MOCK_METHOD(void,
              ShowEditAddressProfileDialog,
              (const autofill::AutofillProfile&),
              (override));
  MOCK_METHOD(void, ShowDeleteAddressProfileDialog, (), (override));
  MOCK_METHOD(bool, HasCreditCardScanFeature, (), (override));
  MOCK_METHOD(void, ScanCreditCard, (CreditCardScanCallback), (override));
  MOCK_METHOD(bool, IsTouchToFillCreditCardSupported, (), (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<autofill::TouchToFillDelegate>,
               base::span<const autofill::CreditCard>),
              (override));
  MOCK_METHOD(void, HideTouchToFillCreditCard, (), (override));
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const PopupOpenArgs&,
               base::WeakPtr<autofill::AutofillPopupDelegate>),
              (override));
  MOCK_METHOD(void,
              UpdateAutofillPopupDataListValues,
              (const std::vector<std::u16string>&,
               const std::vector<std::u16string>&),
              (override));
  MOCK_METHOD(void, PinPopupView, (), (override));
  MOCK_METHOD(PopupOpenArgs,
              GetReopenPopupArgs,
              (autofill::AutofillSuggestionTriggerSource),
              (const, override));
  MOCK_METHOD(std::vector<autofill::Suggestion>,
              GetPopupSuggestions,
              (),
              (const, override));
  MOCK_METHOD(void,
              UpdatePopup,
              (const std::vector<autofill::Suggestion>&,
               autofill::PopupType,
               autofill::AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              HideAutofillPopup,
              (autofill::PopupHidingReason),
              (override));
  MOCK_METHOD(bool, IsAutocompleteEnabled, (), (const, override));
  MOCK_METHOD(bool, IsPasswordManagerEnabled, (), (override));
  MOCK_METHOD(void,
              PropagateAutofillPredictionsDeprecated,
              (autofill::AutofillDriver*,
               const std::vector<autofill::FormStructure*>&),
              (override));
  MOCK_METHOD(void,
              DidFillOrPreviewForm,
              (autofill::mojom::AutofillActionPersistence action_persistence,
               autofill::AutofillTriggerSource trigger_source,
               bool is_refill),
              (override));
  MOCK_METHOD(void,
              DidFillOrPreviewField,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(bool, IsContextSecure, (), (const, override));
  MOCK_METHOD(autofill::LogManager*, GetLogManager, (), (const, override));
  MOCK_METHOD(const autofill::AutofillAblationStudy&,
              GetAblationStudy,
              (),
              (const, override));
#if BUILDFLAG(IS_IOS)
  MOCK_METHOD(bool, IsLastQueriedField, (autofill::FieldGlobalId), (override));
#endif
  MOCK_METHOD(void,
              LoadRiskData,
              (base::OnceCallback<void(const std::string&)>),
              (override));
  MOCK_METHOD(void,
              OpenPromoCodeOfferDetailsURL,
              (const GURL& url),
              (override));
  MOCK_METHOD(autofill::FormInteractionsFlowId,
              GetCurrentFormInteractionsFlowId,
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
    authenticator_ =
        base::MakeRefCounted<device_reauth::MockDeviceAuthenticator>();
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
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kHadBiometricsAvailable, false);
    ON_CALL(mock_client_, GetLocalStatePrefs())
        .WillByDefault(Return(&pref_service_));
    ON_CALL(mock_client_, GetPrefs()).WillByDefault(Return(&pref_service_));
    ON_CALL(mock_client_, GetDeviceAuthenticator())
        .WillByDefault(Return(authenticator_));
    ON_CALL(*authenticator_, CanAuthenticateWithBiometrics)
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
  scoped_refptr<device_reauth::MockDeviceAuthenticator> authenticator_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

TEST(PasswordManagerUtil, TrimUsernameOnlyCredentials) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  forms.push_back(std::make_unique<PasswordForm>(GetTestAndroidCredential()));
  expected_forms.push_back(
      std::make_unique<PasswordForm>(GetTestAndroidCredential()));

  PasswordForm username_only;
  username_only.scheme = PasswordForm::Scheme::kUsernameOnly;
  username_only.signon_realm = kTestAndroidRealm;
  username_only.username_value = kTestUsername2;
  forms.push_back(std::make_unique<PasswordForm>(username_only));

  username_only.federation_origin =
      url::Origin::Create(GURL(kTestFederationURL));
  username_only.skip_zero_click = false;
  forms.push_back(std::make_unique<PasswordForm>(username_only));
  username_only.skip_zero_click = true;
  expected_forms.push_back(std::make_unique<PasswordForm>(username_only));

  TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

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
  password_manager::StubPasswordManagerClient stub_password_client;
  MockAutofillClient mock_autofill_client;

  EXPECT_CALL(mock_autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::
                                    kOverlappingWithPasswordGenerationPopup));

  UserTriggeredManualGenerationFromContextMenu(&stub_password_client,
                                               &mock_autofill_client);
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

TEST(PasswordManagerUtil, IsValidPasswordURL) {
  std::vector<std::pair<GURL, bool>> test_cases = {
      {GURL("noscheme.com"), false},
      {GURL("https://;/invalid"), false},
      {GURL("scheme://unsupported"), false},
      {GURL("http://example.com"), true},
      {GURL("https://test.com/login"), true}};
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.second, IsValidPasswordURL(test_case.first));
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);

  SetBiometricAuthenticationBeforeFilling(/*available=*/false);
  EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));
  EXPECT_FALSE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

TEST_F(PasswordManagerUtilTest, BiometricForFillingFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);
  SetBiometricAuthenticationBeforeFilling(/*available=*/false);
  EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_FALSE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

TEST_F(PasswordManagerUtilTest, BiometricForFillingEnabed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);
  SetBiometricAuthenticationBeforeFilling(/*available=*/true);
  EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_FALSE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

TEST_F(PasswordManagerUtilTest, ShouldShowBiometricAuthPromo) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);
  SetBiometricAuthenticationBeforeFilling(/*available=*/false);
  EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_TRUE(
      ShouldShowBiometricAuthenticationBeforeFillingPromo(&mock_client_));
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

struct TestCase {
  std::string url;
  std::string expected_result;
};

class PasswordManagerUtilMainDomainTest
    : public testing::Test,
      public testing::WithParamInterface<TestCase> {
 protected:
  const base::flat_set<std::string>& psl_extension_list() {
    return psl_extension_list_;
  }

 private:
  base::flat_set<std::string> psl_extension_list_ = {
      "app.link",
      "bttn.io",
      "test-app.link",
      "smart.link",
      "page.link",
      "onelink.me",
      "goo.gl",
      "app.goo.gl",
      "more.app.goo.gl",
      // Missing domain.goo.gl on purpose to show all levels need to be included
      // for multi-level extended main domain (see b/196013199#comment4 for more
      // context)
      "included.domain.goo.gl",
  };
};

TEST_P(PasswordManagerUtilMainDomainTest, ParamTest) {
  const TestCase& tc = GetParam();
  EXPECT_THAT(GetExtendedTopLevelDomain(GURL(tc.url), psl_extension_list()),
              testing::Eq(tc.expected_result));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordManagerUtilMainDomainTest,
    ::testing::Values(
        // error cases
        TestCase(),                         // empty string
        TestCase{"some arbitrary string"},  // not parsable
        TestCase{"amazon.com"},             // no schema
        TestCase{"https://"},               // empty host
        TestCase{"https://.com"},           // Not under psl, too short
        TestCase{"https://192.168.100.1"},  // ip as hostname
        // In PSL list or unknown domain
        TestCase{"https://main.unknown", "main.unknown"},  // unknown domain
        // Blogspot.com, special case which is in PSL
        TestCase{"https://foo.blogspot.com", "foo.blogspot.com"},
        // different url depths
        TestCase{"https://f.com", "f.com"},
        TestCase{"https://facebook.com", "facebook.com"},
        TestCase{"https://www.facebook.com", "facebook.com"},
        TestCase{"https://many.many.many.facebook.com", "facebook.com"},
        // different url schemas and non tld parts
        TestCase{"http://www.twitter.com", "twitter.com"},
        TestCase{"https://mobile.twitter.com", "twitter.com"},
        TestCase{"android://blabla@com.twitter.android"},
        // additional URI components, see
        // https://tools.ietf.org/html/rfc3986#section-3
        TestCase{"https://facebook.com/", "facebook.com"},
        TestCase{"https://facebook.com/path/", "facebook.com"},
        TestCase{"https://facebook.com?queryparam=value", "facebook.com"},
        TestCase{"https://facebook.com#fragment", "facebook.com"},
        TestCase{"https://userinfo@facebook.com", "facebook.com"},
        // public suffix with more than one component
        TestCase{"https://facebook.co.uk", "facebook.co.uk"},
        TestCase{"https://www.some.trentinosuedtirol.it",
                 "some.trentinosuedtirol.it"},
        TestCase{"https://www.some.ac.gov.br", "some.ac.gov.br"},
        // extended top level domains
        TestCase{"https://app.link", "app.link"},
        TestCase{"https://user1.app.link", "user1.app.link"},
        TestCase{"https://user1.test-app.link", "user1.test-app.link"},
        TestCase{"https://many.many.many.user1.app.link", "user1.app.link"},
        // multi level extended top level domains (see b/196013199 and
        // http://doc/1LlPX9DxrCZxsuB_b52vCdiGavVupaI9zjiibdQb9v24)
        TestCase{"https://goo.gl", "goo.gl"},
        TestCase{"https://app.goo.gl", "app.goo.gl"},
        TestCase{"https://user1.app.goo.gl", "user1.app.goo.gl"},
        TestCase{"https://many.many.many.user1.app.goo.gl", "user1.app.goo.gl"},
        TestCase{"https://one.more.app.goo.gl", "one.more.app.goo.gl"},
        // PSL_EXTENSION_LIST contains included.domain.goo.gl but missing
        // domain.goo.gl due to this multi level extension does not extend
        // beyond this level.
        TestCase{"https://levels.not.included.domain.goo.gl", "domain.goo.gl"},
        // Http schema
        TestCase{"http://f.com", "f.com"},
        TestCase{"http://facebook.com", "facebook.com"},
        TestCase{"http://www.facebook.com", "facebook.com"},
        TestCase{"http://many.many.many.facebook.com", "facebook.com"}));

struct MergeRelatedGroupsTestCase {
  std::vector<std::vector<std::string>> input_groups;
  std::vector<std::vector<std::string>> output_groups;
  std::vector<std::string> psl_extensions;
};

class PasswordManagerUtilMergeRelatedGroupsTest
    : public testing::Test,
      public testing::WithParamInterface<MergeRelatedGroupsTestCase> {
 protected:
  std::vector<GroupedFacets> GetGroups(
      const std::vector<std::vector<std::string>>& groups) {
    std::vector<password_manager::GroupedFacets> results;
    for (const auto& group : groups) {
      GroupedFacets result;
      for (const auto& facet : group) {
        result.facets.emplace_back(
            FacetURI::FromPotentiallyInvalidSpec("https://" + facet));
      }
      results.push_back(std::move(result));
    }
    return results;
  }

  void SortFacets(std::vector<GroupedFacets>& groups) {
    for (auto& group : groups) {
      std::sort(group.facets.begin(), group.facets.end(),
                [](const auto& lhs, const auto& rhs) {
                  return base::CompareCaseInsensitiveASCII(
                      lhs.uri.potentially_invalid_spec(),
                      rhs.uri.potentially_invalid_spec());
                });
    }
  }

  base::flat_set<std::string> GetPSLExtensions() {
    return base::flat_set<std::string>(GetParam().psl_extensions);
  }
};

TEST_P(PasswordManagerUtilMergeRelatedGroupsTest, ParamTest) {
  std::vector<GroupedFacets> expected_groups =
      GetGroups(GetParam().output_groups);
  std::vector<GroupedFacets> actual_groups = MergeRelatedGroups(
      GetPSLExtensions(), GetGroups(GetParam().input_groups));

  // Sort facets to simplify testing as their order doesn'r matter
  SortFacets(expected_groups);
  SortFacets(actual_groups);

  EXPECT_THAT(actual_groups,
              testing::UnorderedElementsAreArray(expected_groups));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordManagerUtilMergeRelatedGroupsTest,
    ::testing::Values(
        MergeRelatedGroupsTestCase{{{"a.com"}, {"b.com"}, {"c.com"}},
                                   {{"a.com"}, {"b.com"}, {"c.com"}},
                                   {}},
        MergeRelatedGroupsTestCase{
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {{"a.com", "test1.a.com", "test2.a.com"}},
            {}},
        // When a.com is extended to be a public suffix the groups no longer
        // merge together.
        MergeRelatedGroupsTestCase{
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {{"a.com"}, {"test1.a.com"}, {"test2.a.com"}},
            {"a.com"}},
        MergeRelatedGroupsTestCase{{{"a.com", "b.com"}, {"www.b.com", "c.com"}},
                                   {{"a.com", "b.com", "www.b.com", "c.com"}},
                                   {}},
        MergeRelatedGroupsTestCase{
            {{"a.com", "b.com"}, {"www.b.com", "c.com"}, {"d.org"}},
            {{"a.com", "b.com", "www.b.com", "c.com"}, {"d.org"}},
            {}},
        MergeRelatedGroupsTestCase{
            {{"a.com", "b.com", "c.com"},
             {"www.b.com"},
             {"d.org", "www.c.com"}},
            {{"a.com", "b.com", "c.com", "www.b.com", "d.org", "www.c.com"}},
            {}}

        ));

}  // namespace password_manager_util
