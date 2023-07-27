// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#endif

// The name of the username/password element in the form.
const char16_t kInvalidUsername[] = u"no-username";

const char16_t kAliceUsername[] = u"alice";
const char16_t kAlicePassword[] = u"password";
const char16_t kAliceAccountStoredPassword[] = u"account-stored-password";

using autofill::PopupType;
using autofill::Suggestion;
using autofill::SuggestionVectorIconsAre;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorLabelsContains;
using autofill::SuggestionVectorMainTextsAre;
using autofill::password_generation::PasswordGenerationType;
using base::test::RunOnceCallback;
using device_reauth::DeviceAuthRequester;
using favicon_base::FaviconImageCallback;
using gfx::test::AreImagesEqual;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SizeIs;
using testing::Unused;

using UkmEntry = ukm::builders::PageWithPassword;

namespace autofill {
class AutofillPopupDelegate;
class PrefService;
}

namespace password_manager {

namespace {

using ReauthSucceeded = PasswordManagerClient::ReauthSucceeded;

constexpr char kMainFrameUrl[] = "https://example.com/";
constexpr char kDropdownSelectedHistogram[] =
    "PasswordManager.PasswordDropdownItemSelected";
constexpr char kDropdownShownHistogram[] =
    "PasswordManager.PasswordDropdownShown";
constexpr char kCredentialsCountFromAccountStoreAfterUnlockHistogram[] =
    "PasswordManager.CredentialsCountFromAccountStoreAfterUnlock";
const gfx::Image kTestFavicon = gfx::test::CreateImage(16, 16);

const Suggestion::Text kSeparatorEntry(u"", Suggestion::Text::IsPrimary(false));

constexpr autofill::FieldRendererId kElementId;

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              SetSuggestionAvailability,
              (autofill::FieldRendererId element_id,
               const autofill::mojom::AutofillState state),
              (override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (override));
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const override));
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient() : main_frame_url_(kMainFrameUrl) {}

  MockPasswordManagerDriver* mock_driver() { return &driver_; }
  const GURL& GetLastCommittedURL() const override { return main_frame_url_; }

  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override {
    return feature_manager_.get();
  }

  MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return feature_manager_.get();
  }

  signin::IdentityManager* GetIdentityManager() override {
    return identity_test_env_.identity_manager();
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  void SetAccountStorageOptIn(bool opt_in) {
    ON_CALL(*feature_manager_.get(), ShouldShowAccountStorageOptIn)
        .WillByDefault(Return(!opt_in));
  }

  void SetNeedsReSigninForAccountStorage(bool needs_signin) {
    ON_CALL(*feature_manager_.get(), ShouldShowAccountStorageReSignin)
        .WillByDefault(Return(needs_signin));
  }

  void SetDeviceAuthenticator(
      scoped_refptr<device_reauth::MockDeviceAuthenticator>
          biometric_authenticator) {
    biometric_authenticator_ = std::move(biometric_authenticator);
  }

  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override {
    return biometric_authenticator_;
  }

  MOCK_METHOD(void, GeneratePassword, (PasswordGenerationType), (override));
  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(ReauthSucceeded)>),
              (override));
  MOCK_METHOD(void, TriggerSignIn, (signin_metrics::AccessPoint), (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(PrefService*, GetLocalStatePrefs, (), (const, override));

 private:
  MockPasswordManagerDriver driver_;
  scoped_refptr<device_reauth::MockDeviceAuthenticator>
      biometric_authenticator_ = nullptr;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockPasswordFeatureManager> feature_manager_{
      new NiceMock<MockPasswordFeatureManager>};
  GURL main_frame_url_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const autofill::AutofillClient::PopupOpenArgs& open_args,
               base::WeakPtr<autofill::AutofillPopupDelegate> delegate),
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
               PopupType,
               autofill::AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              HideAutofillPopup,
              (autofill::PopupHidingReason),
              (override));
};

base::CancelableTaskTracker::TaskId
RespondWithTestIcon(Unused, FaviconImageCallback callback, Unused) {
  favicon_base::FaviconImageResult image_result;
  image_result.image = kTestFavicon;
  std::move(callback).Run(image_result);
  return 1;
}

std::vector<autofill::Suggestion> CreateTestSuggestions(
    bool has_opt_in_and_fill,
    bool has_opt_in_and_generate,
    bool has_re_signin) {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(
      /*value=*/"User1", /*label=*/"PW1", /*icon=*/"",
      /*popup_item_id=*/autofill::PopupItemId::kPasswordEntry);
  suggestions.emplace_back(
      /*value=*/"Show all pwds", /*label=*/"", /*icon=*/"",
      /*popup_item_id=*/autofill::PopupItemId::kAllSavedPasswordsEntry);
  if (has_opt_in_and_fill) {
    suggestions.emplace_back(
        /*value=*/"Unlock passwords and fill", /*label=*/"", /*icon=*/"",
        /*popup_item_id=*/
        autofill::PopupItemId::kPasswordAccountStorageOptIn);
  }
  if (has_opt_in_and_generate) {
    suggestions.emplace_back(
        /*value=*/"Unlock passwords and generate", /*label=*/"", /*icon=*/"",
        /*popup_item_id=*/
        autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate);
  }
  if (has_re_signin) {
    suggestions.emplace_back(
        /*value=*/"Sign in to access passwords", /*label=*/"", /*icon=*/"",
        /*popup_item_id=*/
        autofill::PopupItemId::kPasswordAccountStorageReSignin);
  }
  return suggestions;
}

std::vector<autofill::Suggestion> SetLoading(
    std::vector<autofill::Suggestion> suggestions,
    int index_of_loading_element) {
  suggestions[index_of_loading_element].is_loading =
      Suggestion::IsLoading(true);
  return suggestions;
}

autofill::AutofillClient::PopupOpenArgs CreateReopenArgsWithTestSuggestions(
    bool has_opt_in_and_fill,
    bool has_opt_in_and_generate,
    bool has_re_signin) {
  return {gfx::RectF(), base::i18n::LEFT_TO_RIGHT,
          CreateTestSuggestions(has_opt_in_and_fill, has_opt_in_and_generate,
                                has_re_signin),
          autofill::AutofillSuggestionTriggerSource::kPasswordManager};
}

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(kAliceUsername), test_password_(kAlicePassword) {}

  void SetUp() override {
    // Add a preferred login.
    fill_data_.preferred_login.username_value = test_username_;
    fill_data_.preferred_login.password_value = test_password_;
  }

  void InitializePasswordAutofillManager(TestPasswordManagerClient* client,
                                         MockAutofillClient* autofill_client) {
    password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
        client->mock_driver(), autofill_client, client);
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(*client, GetFaviconService())
        .WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service,
                GetFaviconImageForPageURL(fill_data_.url, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(fill_data_);
    testing::Mock::VerifyAndClearExpectations(client);
    // Suppress the warnings in the tests.
    EXPECT_CALL(*client, GetFaviconService()).WillRepeatedly(Return(nullptr));

    webauthn_credentials_delegate_ =
        std::make_unique<MockWebAuthnCredentialsDelegate>();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_->registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
    ON_CALL(*client, GetPrefs())
        .WillByDefault(Return(test_pref_service_.get()));
    ON_CALL(*client, GetLocalStatePrefs())
        .WillByDefault(Return(test_pref_service_.get()));
#endif
    ON_CALL(*client, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(webauthn_credentials_delegate_.get()));

    EXPECT_CALL(*client->mock_driver(), CanShowAutofillUi)
        .WillRepeatedly(Return(true));
  }

  autofill::PasswordFormFillData CreateTestFormFillData() {
    autofill::PasswordFormFillData data;
    data.preferred_login.username_value = test_username_;
    data.preferred_login.password_value = test_password_;
    data.preferred_login.realm = "http://foo.com/";
    return data;
  }

  std::u16string GetManagePasswordsTitle() {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS);
  }

  std::string GetManagePasswordsIcon() {
    // The "Manage passwords" entry only has an icon if
    // kEnablePasswordsAccountStorage is enabled.
    std::string settings_icon;
    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      return "settingsIcon";
    }
    return std::string();
  }

  void ExpectAndAllowAuthentication() {
    // Allow authentication.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    EXPECT_CALL(*authenticator_.get(), AuthenticateWithMessage);
#else
    EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
        .WillOnce(Return(true));
    EXPECT_CALL(*authenticator_.get(),
                Authenticate(DeviceAuthRequester::kAutofillSuggestion, _,
                             /*use_last_valid_auth= */ true));
#endif
  }

 protected:
  autofill::PasswordFormFillData& fill_data() { return fill_data_; }

  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;

  scoped_refptr<device_reauth::MockDeviceAuthenticator> authenticator_ =
      base::MakeRefCounted<device_reauth::MockDeviceAuthenticator>();

  std::unique_ptr<MockWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;

  std::u16string test_username_;
  std::u16string test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::test::SingleThreadTaskEnvironment task_environment_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
#endif
};

TEST_F(PasswordAutofillManagerTest, FillSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->FillSuggestionForTest(kInvalidUsername));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
}

TEST_F(PasswordAutofillManagerTest, PreviewSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), PreviewSuggestion(_, _)).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(kInvalidUsername));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
}

// Test that the popup is marked as visible after receiving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling and favicon data.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    data.preferred_login.uses_account_store = false;
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify the suggestions.
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
        show_suggestion_options, gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(
        open_args.suggestions,
        SuggestionVectorIdsAre(is_suggestion_on_password_field
                                   ? autofill::PopupItemId::kPasswordEntry
                                   : autofill::PopupItemId::kUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                               autofill::PopupItemId::kSeparator,
#endif
                               autofill::PopupItemId::kAllSavedPasswordsEntry));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));

    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));
    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    base::HistogramTester histograms;

    password_autofill_manager_->DidAcceptSuggestion(
        autofill::test::CreateAutofillSuggestion(
            is_suggestion_on_password_field
                ? autofill::PopupItemId::kPasswordEntry
                : autofill::PopupItemId::kUsernameEntry,
            test_username_),
        1);

    histograms.ExpectUniqueSample(
        kDropdownSelectedHistogram,
        metrics_util::PasswordDropdownSelectedOption::kPassword, 1);
  }
}

// Test that suggestions are filled correctly if account-stored and local
// suggestion have duplicates.
TEST_F(PasswordAutofillManagerTest,
       ExternalDelegateAccountStorePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling data and account-stored duplicate with a different password.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    autofill::PasswordAndMetadata duplicate;
    duplicate.password_value = kAliceAccountStoredPassword;
    duplicate.realm = data.preferred_login.realm;
    duplicate.uses_account_store = true;
    duplicate.username_value = data.preferred_login.username_value;
    data.additional_logins.push_back(duplicate);
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify local and account-stored suggestion coexist.
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));
    password_autofill_manager_->OnShowPasswordSuggestions(
        kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
        is_suggestion_on_password_field
            ? autofill::IS_PASSWORD_FIELD
            : autofill::ShowPasswordSuggestionsOptions(),
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 2u);
    EXPECT_THAT(open_args.suggestions,
                SuggestionVectorIdsAre(
                    is_suggestion_on_password_field
                        ? autofill::PopupItemId::kPasswordEntry
                        : autofill::PopupItemId::kUsernameEntry,
                    is_suggestion_on_password_field
                        ? autofill::PopupItemId::kAccountStoragePasswordEntry
                        : autofill::PopupItemId::kAccountStorageUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                    autofill::PopupItemId::kSeparator,
#endif
                    autofill::PopupItemId::kAllSavedPasswordsEntry));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[1].custom_icon, kTestFavicon));

    // When selecting the account-stored credential, make sure the filled
    // password belongs to the selected credential (and not to the first match).
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, duplicate.password_value));
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

    password_autofill_manager_->DidAcceptSuggestion(
        autofill::test::CreateAutofillSuggestion(
            is_suggestion_on_password_field
                ? autofill::PopupItemId::kAccountStoragePasswordEntry
                : autofill::PopupItemId::kAccountStorageUsernameEntry,
            test_username_),
        /*position=*/1);
  }
}

// Test that the popup is updated once account-stored suggestions are unlocked.
TEST_F(PasswordAutofillManagerTest, ShowOptInAndFillButton) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kPasswordEntry,
                  autofill::PopupItemId::kPasswordAccountStorageOptIn,
#if !BUILDFLAG(IS_ANDROID)
                  autofill::PopupItemId::kSeparator,
#endif
                  autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Test that a popup without entries doesn't show "Manage all Passwords".
TEST_F(PasswordAutofillManagerTest, SuppressManageAllWithoutPasswords) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
      client.mock_driver(), &autofill_client, &client);
  client.SetAccountStorageOptIn(false);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kPasswordAccountStorageOptIn));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Test that the popup is updated once account-stored suggestions are unlocked.
TEST_F(PasswordAutofillManagerTest, ShowResigninButton) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetNeedsReSigninForAccountStorage(true);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kPasswordEntry,
                  autofill::PopupItemId::kPasswordAccountStorageReSignin,
#if !BUILDFLAG(IS_ANDROID)
                  autofill::PopupItemId::kSeparator,
#endif
                  autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndFillPutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock button into a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(
                      autofill::PopupItemId::kPasswordEntry,
                      autofill::PopupItemId::kAllSavedPasswordsEntry,
                      autofill::PopupItemId::kPasswordAccountStorageOptIn),
                  PopupType::kPasswords,
                  autofill::AutofillSuggestionTriggerSource::kPasswordManager))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(client, TriggerReauthForPrimaryAccount);
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptIn, test_username_),
      1);

  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndGeneratePutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock-to-generate button in a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(
          SuggestionVectorIdsAre(
              autofill::PopupItemId::kPasswordEntry,
              autofill::PopupItemId::kAllSavedPasswordsEntry,
              autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate),
          PopupType::kPasswords,
          autofill::AutofillSuggestionTriggerSource::kPasswordManager))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(client, TriggerReauthForPrimaryAccount);
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate,
          test_username_),
      1);

  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest, ClickOnReSiginTriggersSigninAndHides) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetNeedsReSigninForAccountStorage(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  EXPECT_CALL(client,
              TriggerSignIn(
                  signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN));
  EXPECT_CALL(autofill_client, HideAutofillPopup);

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageReSignin,
          test_username_),
      1);
}

// Test that the popup is updated once "opt in and fill" is clicked and the
// reauth fails.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndFillUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));
  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, UpdatePopup).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetReopenPopupArgs)
        .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
            /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
            /*has_re_signin=*/false)));
    EXPECT_CALL(client,
                TriggerReauthForPrimaryAccount(
                    signin_metrics::ReauthAccessPoint::kAutofillDropdown, _))
        .WillOnce([](auto, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(autofill_client, ShowAutofillPopup);
    EXPECT_CALL(autofill_client, PinPopupView);
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(
                autofill::PopupItemId::kPasswordEntry,
                autofill::PopupItemId::kAllSavedPasswordsEntry,
                autofill::PopupItemId::kPasswordAccountStorageOptIn),
            PopupType::kPasswords,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptIn, test_username_),
      1);

  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked and the
// reauth fails.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndGenerateUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));
  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, UpdatePopup).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetReopenPopupArgs)
        .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
            /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true,
            /*has_re_signin=*/false)));
    EXPECT_CALL(
        client,
        TriggerReauthForPrimaryAccount(
            signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown, _))
        .WillOnce([](auto, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(autofill_client, ShowAutofillPopup);
    EXPECT_CALL(autofill_client, PinPopupView);
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(
                autofill::PopupItemId::kPasswordEntry,
                autofill::PopupItemId::kAllSavedPasswordsEntry,
                autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate),
            PopupType::kPasswords,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate,
          test_username_),
      1);

  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked and the
// reauth is successful.
TEST_F(PasswordAutofillManagerTest, SuccessfullOptInAndFillHidesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, GetReopenPopupArgs)
      .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
          /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
          /*has_re_signin=*/false)));
  EXPECT_CALL(client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kAutofillDropdown, _))
      .WillOnce([](auto, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  EXPECT_CALL(autofill_client, PinPopupView);

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptIn, test_username_),
      1);
}

// Test that the popup is hidden and password generation is triggered once
// "opt in and generate" is clicked and the reauth is successful.
TEST_F(PasswordAutofillManagerTest,
       SuccessfullOptInAndGenerateHidesPopupAndTriggersGeneration) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, GetReopenPopupArgs)
      .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true,
          /*has_re_signin=*/false)));
  EXPECT_CALL(
      client,
      TriggerReauthForPrimaryAccount(
          signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown, _))
      .WillOnce([](auto, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  EXPECT_CALL(autofill_client, PinPopupView);
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(client, GeneratePassword(PasswordGenerationType::kAutomatic));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate,
          test_username_),
      1);
}

// Test that the popup shows an empty state if opted-into an empty store.
TEST_F(PasswordAutofillManagerTest, SuccessfullOptInMayShowEmptyState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  base::HistogramTester histograms;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(true);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Only the unlock button was available. After being clicked, it's in a
  // loading state which the DeleteFillData() call will end.
  Suggestion unlock_suggestion(
      /*main_text=*/"Unlock passwords and fill", /*label=*/"", /*icon=*/"",
      /*popup_item_id=*/
      autofill::PopupItemId::kPasswordAccountStorageOptIn);
  unlock_suggestion.is_loading = Suggestion::IsLoading(true);
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillRepeatedly(Return(std::vector<Suggestion>{unlock_suggestion}));
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kStaleData));
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(
                      autofill::PopupItemId::kPasswordAccountStorageEmpty),
                  PopupType::kPasswords,
                  autofill::AutofillSuggestionTriggerSource::kPasswordManager));

  password_autofill_manager_->DeleteFillData();
  password_autofill_manager_->OnNoCredentialsFound();
  histograms.ExpectBucketCount(
      kCredentialsCountFromAccountStoreAfterUnlockHistogram, 0, 1);
}

// Test that the popup is updated once "opt in and fill" is clicked".
TEST_F(PasswordAutofillManagerTest,
       AddOnFillDataAfterOptInAndFillPopulatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  base::HistogramTester histograms;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(true);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Once the data is loaded, an update fills the new passwords:
  autofill::PasswordFormFillData new_data = CreateTestFormFillData();
  new_data.preferred_login.uses_account_store = true;
  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"bar.foo@example.com";
  new_data.additional_logins.push_back(std::move(additional));
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillRepeatedly(Return(SetLoading(
          CreateTestSuggestions(
              /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
              /*has_re_signin=*/false),
          /*index_of_loading_element=*/2)));  // Opt-in is at third position.
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kStaleData));
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(
                      autofill::PopupItemId::kAccountStoragePasswordEntry,
                      autofill::PopupItemId::kPasswordEntry,
#if !BUILDFLAG(IS_ANDROID)
                      autofill::PopupItemId::kSeparator,
#endif
                      autofill::PopupItemId::kAllSavedPasswordsEntry),
                  PopupType::kPasswords,
                  autofill::AutofillSuggestionTriggerSource::kPasswordManager));

  password_autofill_manager_->DeleteFillData();
  password_autofill_manager_->OnAddPasswordFillData(new_data);
  histograms.ExpectBucketCount(
      kCredentialsCountFromAccountStoreAfterUnlockHistogram, 1, 1);
}

// Test that OnShowPasswordSuggestions correctly matches the given FormFieldData
// to the known PasswordFormFillData, and extracts the right suggestions.
TEST_F(PasswordAutofillManagerTest, ExtractSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"John Foo";
  data.additional_logins.push_back(additional);

  autofill::PasswordAndMetadata additional1;
  additional1.realm = "https://foobarrealm.org";
  additional1.username_value = u"Kohn Foo";
  data.additional_logins.push_back(std::move(additional1));

  autofill::PasswordAndMetadata additional2;
  additional2.realm = "https://foobarrealm.org";
  additional2.username_value = u"Gohn Foo";
  data.additional_logins.push_back(std::move(additional2));

  password_autofill_manager_->OnAddPasswordFillData(data);

  // First, simulate displaying suggestions matching an empty prefix. Also
  // verify that both the values and labels are filled correctly. The 'value'
  // should be the user name; the 'label' should be the realm.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Gohn Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"John Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Kohn Foo", Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorLabelsContains(std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(u"foo.com")}}));
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorLabelsContains(std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(u"foobarrealm.org")}}));

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"John",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(additional.username_value,
                                   Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
                  kSeparatorEntry,
#endif
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"xyz", autofill::SHOW_ALL,
      element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Gohn Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"John Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Kohn Foo", Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));
}

// Verify that, for Android application credentials, the prettified realms of
// applications are displayed as the labels of suggestions on the UI (for
// matches of all levels of preferredness).
TEST_F(PasswordAutofillManagerTest, PrettifiedAndroidRealmsAreShownAsLabels) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data;
  data.preferred_login.username_value = test_username_;
  data.preferred_login.realm = "android://hash@com.example1.android/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "android://hash@com.example2.android/";
  additional.username_value = u"John Foo";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), gfx::RectF());
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorLabelsContains(std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(u"android://com.example2.android/")}}));
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorLabelsContains(std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(u"android://com.example1.android/")}}));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

TEST_F(PasswordAutofillManagerTest, FillSuggestionPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"John Foo";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Verify that typing "foo" into the username field will match usernames
// "foo.bar@example.com", "bar.foo@example.com" and "example@foo.com".
TEST_F(PasswordAutofillManagerTest, DisplaySuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.preferred_login.username_value = username;
  data.preferred_login.password_value = u"foobar";
  data.preferred_login.realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"foo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(username, Suggestion::Text::IsPrimary(true)),
                  Suggestion::Text(additional.username_value,
                                   Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
                  kSeparatorEntry,
#endif
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Verify that typing "oo" into the username field will not match any usernames
// "foo.bar@example.com", "bar.foo@example.com" or "example@foo.com".
TEST_F(PasswordAutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.preferred_login.username_value = username;
  data.preferred_login.password_value = u"foobar";
  data.preferred_login.realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"bar.foo@example.com";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"oo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
}

// Verify that typing "foo@exam" into the username field will match username
// "bar.foo@example.com" even if the field contents span accross multiple
// tokens.
TEST_F(PasswordAutofillManagerTest,
       MatchingContentsWithSuggestionTokenSeparator) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.preferred_login.username_value = username;
  data.preferred_login.password_value = u"foobar";
  data.preferred_login.realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"foo@exam",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(additional.username_value,
                                   Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
                  kSeparatorEntry,
#endif
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

// Verify that typing "example" into the username field will match and order
// usernames "example@foo.com", "foo.bar@example.com" and "bar.foo@example.com"
// i.e. prefix matched followed by substring matched.
TEST_F(PasswordAutofillManagerTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.preferred_login.username_value = username;
  data.preferred_login.password_value = u"foobar";
  data.preferred_login.realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username_value = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, u"foo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(username, Suggestion::Text::IsPrimary(true)),
                  Suggestion::Text(additional.username_value,
                                   Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
                  kSeparatorEntry,
#endif
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

TEST_F(PasswordAutofillManagerTest, PreviewAndFillEmptyUsernameSuggestion) {
  // Initialize PasswordAutofillManager with credentials without username.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  fill_data().preferred_login.username_value.clear();
  InitializePasswordAutofillManager(&client, &autofill_client);

  std::u16string no_username_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);

  // Simulate that the user clicks on a username field.
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);

  // Check that preview of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(std::u16string(), test_password_));
  const Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      autofill::PopupItemId::kPasswordEntry, no_username_string);
  password_autofill_manager_->DidSelectSuggestion(suggestion);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that fill of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(std::u16string(), test_password_));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(suggestion, /*position=*/1);

  testing::Mock::VerifyAndClearExpectations(client.mock_driver());
}

// Tests that the "Manage passwords" suggestion is shown along with the password
// popup.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnPasswordField) {
  base::HistogramTester histograms;

  NiceMock<MockAutofillClient> autofill_client;
  auto client = std::make_unique<TestPasswordManagerClient>();
  auto manager = std::make_unique<PasswordManager>(client.get());
  InitializePasswordAutofillManager(client.get(), &autofill_client);

  ON_CALL(*(client->mock_driver()), GetPasswordManager())
      .WillByDefault(testing::Return(manager.get()));

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
  histograms.ExpectUniqueSample(kDropdownShownHistogram,
                                metrics_util::PasswordDropdownState::kStandard,
                                1);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));

  // Clicking at the "Show all passwords row" should trigger a call to open
  // the Password Manager settings page and hide the popup.
  EXPECT_CALL(*client, NavigateToManagePasswordsPage(
                           ManagePasswordsReferrer::kPasswordDropdown));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(
      Suggestion(autofill::PopupItemId::kAllSavedPasswordsEntry), 0);

  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 1);
  // Trigger UKM reporting, which happens at destruction time.
  ukm::SourceId expected_source_id = client->GetUkmSourceId();
  // Manually reset the `PasswordAutofillManager`, since it keeps pointers to
  // manager and client.
  password_autofill_manager_.reset();
  manager.reset();
  client.reset();

  const auto& entries = autofill_client.GetTestUkmRecorder()->GetEntriesByName(
      UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(expected_source_id, entry->source_id);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, UkmEntry::kPageLevelUserActionName,
        static_cast<int64_t>(
            PasswordManagerMetricsRecorder::PageLevelUserAction::
                kShowAllPasswordsWhileSomeAreSuggested));
  }
}

// Tests that the "Manage passwords" fallback shows up in non-password
// fields of login forms.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnNonPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationNoCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
      client.mock_driver(), &autofill_client, &client);

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  gfx::RectF element_bounds;
  EXPECT_FALSE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationSomeCredentials) {
  base::HistogramTester histograms;
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Bring up the drop-down with the generation option.
  std::u16string generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
  histograms.ExpectUniqueSample(
      kDropdownShownHistogram,
      metrics_util::PasswordDropdownState::kStandardGenerate, 1);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre("globeIcon", "keyIcon",
#if !BUILDFLAG(IS_ANDROID)
                                       "",
#endif

                                       GetManagePasswordsIcon()));
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(generation_string,
                           Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          kSeparatorEntry,
#endif
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));

  // Click "Generate password".
  EXPECT_CALL(client, GeneratePassword(PasswordGenerationType::kAutomatic));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(
      Suggestion(autofill::PopupItemId::kGeneratePasswordEntry), 1);

  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kGenerate, 1);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithOmittedCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  std::u16string generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre("keyIcon",
#if !BUILDFLAG(IS_ANDROID)
                                       "",
#endif
                                       GetManagePasswordsIcon()));

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(generation_string,
                                   Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
                  kSeparatorEntry,
#endif
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));
}

// Test that if the "opt in and generate" button gets displayed, the regular
// generation button does not.
TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithAccountPasswordsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  client.SetAccountStorageOptIn(false);

  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  auto opt_in_and_generate_id =
      autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate;
  auto regular_generate_id = autofill::PopupItemId::kGeneratePasswordEntry;
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
      gfx::RectF(), base::i18n::RIGHT_TO_LEFT,
      /*show_password_suggestions=*/true);
  EXPECT_THAT(open_args.suggestions,
              Not(Contains(Field(&autofill::Suggestion::popup_item_id,
                                 Eq(regular_generate_id)))));
  EXPECT_THAT(open_args.suggestions,
              Contains(Field(&autofill::Suggestion::popup_item_id,
                             Eq(opt_in_and_generate_id))));
}

TEST_F(PasswordAutofillManagerTest, DisplayAccountSuggestionsIndicatorIcon) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.preferred_login.username_value = test_username_;
  data.preferred_login.password_value = u"foobar";
  data.preferred_login.uses_account_store = true;

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  ASSERT_THAT(open_args.suggestions.size(),
              testing::Ge(1u));  // No footer on Android.
  EXPECT_THAT(open_args.suggestions[0].trailing_icon, "google");
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthNotAvailable) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
        show_suggestion_options, gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(
        open_args.suggestions,
        SuggestionVectorIdsAre(is_suggestion_on_password_field
                                   ? autofill::PopupItemId::kPasswordEntry
                                   : autofill::PopupItemId::kUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                               autofill::PopupItemId::kSeparator,
#endif
                               autofill::PopupItemId::kAllSavedPasswordsEntry));

    // Suggestions should always be filled if the authenticator is not available
    // or it cannot be used.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));

#if BUILDFLAG(IS_ANDROID)
    // The authenticator exists, but cannot be used for authentication.
    EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
        .WillOnce(Return(false));
#endif

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        autofill::test::CreateAutofillSuggestion(
            autofill::PopupItemId::kPasswordEntry, test_username_),
        1);
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthSuccessful) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
        show_suggestion_options, gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(
        open_args.suggestions,
        SuggestionVectorIdsAre(is_suggestion_on_password_field
                                   ? autofill::PopupItemId::kPasswordEntry
                                   : autofill::PopupItemId::kUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                               autofill::PopupItemId::kSeparator,
#endif
                               autofill::PopupItemId::kAllSavedPasswordsEntry));

    // The suggestion should be filled if the authentication is successful.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));

    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    EXPECT_CALL(*authenticator_.get(), AuthenticateWithMessage)
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));
    // The authenticator exists and is available.
#else
    EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
        .WillOnce(Return(true));
    EXPECT_CALL(*authenticator_.get(),
                Authenticate(DeviceAuthRequester::kAutofillSuggestion, _,
                             /*use_last_valid_auth= */ true))
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));
#endif

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        autofill::test::CreateAutofillSuggestion(
            autofill::PopupItemId::kPasswordEntry, test_username_),
        1);
  }
}

TEST_F(PasswordAutofillManagerTest, DoesntFillSuggestionIfAuthFailed) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  for (bool is_suggestion_on_password_field : {false, true}) {
    NiceMock<MockAutofillClient> autofill_client;
    client.SetDeviceAuthenticator(authenticator_);

    InitializePasswordAutofillManager(&client, &autofill_client);

    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
        show_suggestion_options, gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(
        open_args.suggestions,
        SuggestionVectorIdsAre(is_suggestion_on_password_field
                                   ? autofill::PopupItemId::kPasswordEntry
                                   : autofill::PopupItemId::kUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                               autofill::PopupItemId::kSeparator,
#endif
                               autofill::PopupItemId::kAllSavedPasswordsEntry));

    // The suggestion should not be filled if the authentication fails.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_))
        .Times(0);
    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

    // The authenticator exists and is available.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    EXPECT_CALL(*authenticator_.get(), AuthenticateWithMessage)
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));
#else
    EXPECT_CALL(*authenticator_.get(), CanAuthenticateWithBiometrics)
        .WillOnce(Return(true));
    EXPECT_CALL(*authenticator_.get(),
                Authenticate(DeviceAuthRequester::kAutofillSuggestion, _,
                             /*use_last_valid_auth= */ true))
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));
#endif

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        autofill::test::CreateAutofillSuggestion(
            autofill::PopupItemId::kPasswordEntry, test_username_),
        1);
  }
}

TEST_F(PasswordAutofillManagerTest, CancelsOngoingBiometricAuthOnDestroy) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::IS_PASSWORD_FIELD, gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorIdsAre(autofill::PopupItemId::kPasswordEntry,
#if !BUILDFLAG(IS_ANDROID)
                             autofill::PopupItemId::kSeparator,
#endif
                             autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  ExpectAndAllowAuthentication();

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(DeviceAuthRequester::kAutofillSuggestion));
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnDeleteFillData) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::IS_PASSWORD_FIELD, gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorIdsAre(autofill::PopupItemId::kPasswordEntry,
#if !BUILDFLAG(IS_ANDROID)
                             autofill::PopupItemId::kSeparator,
#endif
                             autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  ExpectAndAllowAuthentication();

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(DeviceAuthRequester::kAutofillSuggestion));
  password_autofill_manager_->DeleteFillData();
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnFillDataChange) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::IS_PASSWORD_FIELD, gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorIdsAre(autofill::PopupItemId::kPasswordEntry,
#if !BUILDFLAG(IS_ANDROID)
                             autofill::PopupItemId::kSeparator,
#endif
                             autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  ExpectAndAllowAuthentication();

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(DeviceAuthRequester::kAutofillSuggestion));
  password_autofill_manager_->OnAddPasswordFillData(CreateTestFormFillData());
}

TEST_F(PasswordAutofillManagerTest, CancelsOngoingBiometricAuthOnNewRequest) {
  TestPasswordManagerClient client;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
#else
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
#endif
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::IS_PASSWORD_FIELD, gfx::RectF());
  ExpectAndAllowAuthentication();
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  // Triggering new authentication should cancel ongoing authentication.
  EXPECT_CALL(*authenticator_.get(),
              Cancel(DeviceAuthRequester::kAutofillSuggestion));
  ExpectAndAllowAuthentication();
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  // Destroying the manager should cancel ongoing authentication.
  EXPECT_CALL(*authenticator_.get(),
              Cancel(DeviceAuthRequester::kAutofillSuggestion));
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(PasswordAutofillManagerTest, MetricsRecordedForBiometricAuth) {
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  base::HistogramTester histograms;
  TestPasswordManagerClient client;
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup.
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(), 0, gfx::RectF());

  // The authenticator exists and is available.
  base::OnceCallback<void(bool)> auth_callback;
  EXPECT_CALL(*authenticator_.get(), AuthenticateWithMessage)
      .WillOnce(MoveArg<1>(&auth_callback));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::PopupItemId::kPasswordEntry, test_username_),
      1);

  // Simulate successful authentication and expect successful filling.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_));
  std::move(auth_callback).Run(true);

  // Verify reported metrics.
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", true, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime", kMockElapsedTime,
      1);
}
#endif

TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Return a WebAuthn credential.
  const std::vector<uint8_t> kId = {1, 2, 3, 4};
  const std::string kIdBase64 = base::Base64Encode(kId);
  const std::string kNameUtf8 = "nadeshiko@example.com";
  const std::u16string kName = u"nadeshiko@example.com";
  PasskeyCredential passkey(PasskeyCredential::Source::kAndroidPhone,
                            PasskeyCredential::RpId("example.com"),
                            PasskeyCredential::CredentialId(kId),
                            PasskeyCredential::UserId(),
                            PasskeyCredential::Username(kNameUtf8));
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  absl::optional<std::vector<PasskeyCredential>> passkey_list =
      std::vector<PasskeyCredential>{passkey};
  EXPECT_CALL(webauthn_credentials_delegate, GetPasskeys)
      .WillRepeatedly(ReturnRef(passkey_list));
  EXPECT_CALL(webauthn_credentials_delegate,
              OfferPasskeysFromAnotherDeviceOption)
      .WillRepeatedly(Return(true));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kWebauthnCredential,
                  autofill::PopupItemId::kUsernameEntry,
#if !BUILDFLAG(IS_ANDROID)
                  autofill::PopupItemId::kWebauthnSignInWithAnotherDevice,
                  autofill::PopupItemId::kSeparator,
#endif  // !BUILDFLAG(IS_ANDROID)
                  autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(kIdBase64));
  EXPECT_EQ(open_args.suggestions[0].popup_item_id,
            autofill::PopupItemId::kWebauthnCredential);
  EXPECT_EQ(open_args.suggestions[0].main_text.value, kName);
  EXPECT_TRUE(
      AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));
  ASSERT_EQ(open_args.suggestions[0].labels.size(), 1U);
  ASSERT_EQ(open_args.suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(open_args.suggestions[0].labels[0][0].value,
            passkey.GetAuthenticatorLabel());
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that preview of the "username" (i.e. the credential name) works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(kName, /*password=*/std::u16string(u"")));
  const Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      autofill::PopupItemId::kWebauthnCredential, kName,
      autofill::Suggestion::BackendId(kIdBase64));
  password_autofill_manager_->DidSelectSuggestion(suggestion);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that selecting the credential reports back to the client.
  EXPECT_CALL(webauthn_credentials_delegate, SelectPasskey(kIdBase64));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  /*position=*/1);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSignInWithAnotherDevice) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Enable WebAuthn autofill.
  absl::optional<std::vector<PasskeyCredential>> passkeys(absl::in_place);
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  EXPECT_CALL(webauthn_credentials_delegate, GetPasskeys)
      .WillRepeatedly(ReturnRef(passkeys));
  EXPECT_CALL(webauthn_credentials_delegate,
              OfferPasskeysFromAnotherDeviceOption)
      .WillRepeatedly(Return(true));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kUsernameEntry,
                  autofill::PopupItemId::kWebauthnSignInWithAnotherDevice,
                  autofill::PopupItemId::kSeparator,
                  autofill::PopupItemId::kAllSavedPasswordsEntry));

  // Check that the button shows the correct text.
  EXPECT_EQ(open_args.suggestions[1].main_text.value,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DEVICE_PASSKEY));
}

TEST_F(PasswordAutofillManagerTest, DoesntShowWebAuthnSignInWithAnotherDevice) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Enable WebAuthn autofill.
  absl::optional<std::vector<PasskeyCredential>> passkeys(absl::in_place);
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  EXPECT_CALL(webauthn_credentials_delegate, GetPasskeys)
      .WillRepeatedly(ReturnRef(passkeys));
  EXPECT_CALL(webauthn_credentials_delegate,
              OfferPasskeysFromAnotherDeviceOption)
      .WillRepeatedly(Return(false));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(
      open_args.suggestions,
      SuggestionVectorIdsAre(autofill::PopupItemId::kUsernameEntry,
                             autofill::PopupItemId::kSeparator,
                             autofill::PopupItemId::kAllSavedPasswordsEntry));
}

// Regression test for crbug.com/1370037.
TEST_F(PasswordAutofillManagerTest, WebAuthnFaviconWithoutPasswords) {
  // Initialize a PasswordAutofillManager with an empty password form.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
      client.mock_driver(), &autofill_client, &client);
  autofill::PasswordFormFillData data;
  data.url = GURL(kMainFrameUrl);
  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
      .WillOnce(Invoke(RespondWithTestIcon));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Enable WebAuthn autofill to return a credential.
  PasskeyCredential passkey(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("rpid.com"),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId({1, 2, 3, 4}),
      PasskeyCredential::Username("nadeshiko@example.com"));
  absl::optional<std::vector<PasskeyCredential>> passkeys =
      std::vector{std::move(passkey)};
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  EXPECT_CALL(webauthn_credentials_delegate, GetPasskeys)
      .WillRepeatedly(ReturnRef(passkeys));
  EXPECT_CALL(webauthn_credentials_delegate,
              OfferPasskeysFromAnotherDeviceOption)
      .WillRepeatedly(Return(true));

  // Show webauthn suggestions with the correct favicon.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kWebauthnCredential,
                  autofill::PopupItemId::kWebauthnSignInWithAnotherDevice,
                  autofill::PopupItemId::kSeparator,
                  autofill::PopupItemId::kAllSavedPasswordsEntry));
  EXPECT_TRUE(
      AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));
}

// Regression test for crbug.com/1362742.
TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSignInWithoutPasswordData) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // InitializePasswordAutofillManager sets fill data, delete it to simulate
  // having no data.
  password_autofill_manager_->DeleteFillData();

  // Enable WebAuthn autofill.
  absl::optional<std::vector<PasskeyCredential>> passkeys(absl::in_place);
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  EXPECT_CALL(webauthn_credentials_delegate, GetPasskeys)
      .WillRepeatedly(ReturnRef(passkeys));
  EXPECT_CALL(webauthn_credentials_delegate,
              OfferPasskeysFromAnotherDeviceOption)
      .WillRepeatedly(Return(true));

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::PopupItemId::kWebauthnSignInWithAnotherDevice));

  // Check that the button shows the correct text.
  EXPECT_EQ(open_args.suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DEVICE_PASSKEY));
}

TEST_F(PasswordAutofillManagerTest, WebAuthnSignInLaunchesWebAuthnFlow) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Enable WebAuthn autofill.
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegateForDriver)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));

  // Check that selecting the button reports back to the client.
  EXPECT_CALL(webauthn_credentials_delegate, LaunchWebAuthnFlow());
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

  Suggestion suggestion;
  suggestion.main_text.value =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DEVICE_PASSKEY);
  suggestion.popup_item_id =
      autofill::PopupItemId::kWebauthnSignInWithAnotherDevice;
  suggestion.payload = autofill::Suggestion::BackendId();
  password_autofill_manager_->DidAcceptSuggestion(suggestion, /*position=*/1);
}

// Test that the AutofillState is set according to the popup availability.
TEST_F(PasswordAutofillManagerTest, AutofillStateNoSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // No popup, thus kNoSuggestions is set on the field.
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(false));
  EXPECT_CALL(*client.mock_driver(),
              SetSuggestionAvailability(
                  kElementId, autofill::mojom::AutofillState::kNoSuggestions));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
}

// Test that the AutofillState is set according to the popup availability.
TEST_F(PasswordAutofillManagerTest, AutofillStateAutofillAvailable) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // The popup is visible, kAutofillAvailable is set.
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_CALL(
      *client.mock_driver(),
      SetSuggestionAvailability(
          kElementId, autofill::mojom::AutofillState::kAutofillAvailable));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(PasswordAutofillManagerTest, NoPreviewSuggestionWithAuthBeforeFilling) {
  TestPasswordManagerClient client;
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));
  NiceMock<MockAutofillClient> autofill_client;
  client.SetDeviceAuthenticator(authenticator_);
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(), PreviewSuggestion).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());
}
#endif

}  // namespace password_manager
