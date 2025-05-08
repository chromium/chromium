// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/identity_credential/mock_identity_credential_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_suggestion_flow.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "device/fido/features.h"
#include "manage_passwords_referrer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif

namespace autofill {
class AutofillSuggestionDelegate;
}

namespace password_manager {

namespace {

using autofill::FillingProduct;
using autofill::MockIdentityCredentialDelegate;
using autofill::Suggestion;
using autofill::SuggestionAdditionalLabelsContains;
using autofill::SuggestionVectorIconsAre;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorMainTextsAre;
using autofill::password_generation::PasswordGenerationType;
using base::test::RunOnceCallback;
using favicon_base::FaviconImageCallback;
using gfx::test::AreImagesEqual;
using testing::_;
using testing::AllOf;
using testing::DoAll;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::SizeIs;
using testing::Unused;

using ReauthSucceeded = PasswordManagerClient::ReauthSucceeded;
using SuggestionPosition =
    autofill::AutofillSuggestionDelegate::SuggestionMetadata;
using UkmEntry = ukm::builders::PageWithPassword;

// The name of the username/password element in the form.
const char16_t kInvalidUsername[] = u"no-username";
const char16_t kAliceUsername[] = u"alice";
const char16_t kAlicePassword[] = u"password";
const char16_t kAliceAccountStoredPassword[] = u"account-stored-password";

constexpr char kMainFrameUrl[] = "https://example.com/";
constexpr char kDropdownSelectedHistogram[] =
    "PasswordManager.PasswordDropdownItemSelected";
constexpr char kDropdownShownHistogram[] =
    "PasswordManager.PasswordDropdownShown";

// Fields for PasskeyCredentials.
const std::vector<uint8_t> kPasskeyId = {1, 2, 3, 4};
const std::string kPasskeyIdBase64 = base::Base64Encode(kPasskeyId);
constexpr char kPasskeyNameUtf8[] = "nadeshiko@example.com";
const std::u16string kPasskeyName = u"nadeshiko@example.com";

constexpr auto kDefaultTriggerSource =
    autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked;

const gfx::Image kTestFavicon = gfx::test::CreateImage(16, 16);

const Suggestion::Text kSeparatorEntry(u"", Suggestion::Text::IsPrimary(false));

constexpr autofill::FieldRendererId kElementId;

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&,
               const std::u16string&,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              PreviewSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(
      void,
      SetSuggestionAvailability,
      (autofill::FieldRendererId element_id,
       autofill::mojom::AutofillSuggestionAvailability suggestion_availability),
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

  const signin::IdentityManager* GetIdentityManager() const override {
    return identity_test_env_.identity_manager();
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  MOCK_METHOD(void, GeneratePassword, (PasswordGenerationType), (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(
      std::unique_ptr<
          password_manager::PasswordCrossDomainConfirmationPopupController>,
      ShowCrossDomainConfirmationPopup,
      (const gfx::RectF& element_bounds,
       base::i18n::TextDirection text_direction,
       const GURL& domain,
       const std::u16string& password_hostname,
       bool show_warning_text,
       base::OnceClosure confirmation_callback),
      (override));
#endif

 private:
  MockPasswordManagerDriver driver_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockPasswordFeatureManager> feature_manager_{
      new NiceMock<MockPasswordFeatureManager>};
  GURL main_frame_url_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MOCK_METHOD(AutofillClient::SuggestionUiSessionId,
              ShowAutofillSuggestions,
              (const autofill::AutofillClient::PopupOpenArgs& open_args,
               base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate),
              (override));
  MOCK_METHOD(void,
              UpdateAutofillSuggestions,
              (const std::vector<Suggestion>&,
               FillingProduct,
               autofill::AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (autofill::SuggestionHidingReason),
              (override));

  base::span<const Suggestion> GetAutofillSuggestions() const override {
    return autofill_suggestions_;
  }
  void set_autofill_suggestions(std::vector<Suggestion> suggestions) {
    autofill_suggestions_ = std::move(suggestions);
  }

  void CreateTestSuggestions(
      std::optional<size_t> loading_index = std::nullopt) {
    autofill_suggestions_.clear();
    autofill_suggestions_.emplace_back(
        /*value=*/"User1", /*label=*/"PW1", /*icon=*/Suggestion::Icon::kNoIcon,
        /*type=*/autofill::SuggestionType::kPasswordEntry);
    autofill_suggestions_.emplace_back(
        /*value=*/"Show all pwds", /*label=*/"",
        /*icon=*/Suggestion::Icon::kNoIcon,
        /*type=*/
        autofill::SuggestionType::kAllSavedPasswordsEntry);
    if (loading_index) {
      autofill_suggestions_[*loading_index].is_loading =
          Suggestion::IsLoading(true);
    }
  }

 private:
  std::vector<Suggestion> autofill_suggestions_;
};

// Copies the first parameter into `args` and returns a (empty) identifier.
auto SavePopupOpenArgs(autofill::AutofillClient::PopupOpenArgs& args) {
  return DoAll(SaveArg<0>(&args),
               Return(autofill::AutofillClient::SuggestionUiSessionId()));
}

base::CancelableTaskTracker::TaskId
RespondWithTestIcon(Unused, FaviconImageCallback callback, Unused) {
  favicon_base::FaviconImageResult image_result;
  image_result.image = kTestFavicon;
  std::move(callback).Run(image_result);
  return 1;
}

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(kAliceUsername), test_password_(kAlicePassword) {}

  void SetUp() override {
    // Add a preferred login.
    fill_data_.preferred_login.username_value = test_username_;
    fill_data_.preferred_login.password_value = test_password_;
    SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/true);
  }

  void InitializePasswordAutofillManager(TestPasswordManagerClient* client,
                                         MockAutofillClient* autofill_client) {
    password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
        client->mock_driver(), autofill_client, client);
    password_autofill_manager_->SetManualFallbackFlowForTest(
        std::make_unique<NiceMock<MockPasswordSuggestionFlow>>());
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

    ON_CALL(*client, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(webauthn_credentials_delegate_.get()));
    ON_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
        .WillByDefault(Return(false));

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

  void ExpectAndAllowAuthentication(
      device_reauth::MockDeviceAuthenticator* authenticator) {
    // Allow authentication.
    ON_CALL(*authenticator, CanAuthenticateWithBiometrics)
        .WillByDefault(Return(true));
    EXPECT_CALL(*authenticator, AuthenticateWithMessage);
  }

  MockPasswordSuggestionFlow& manual_fallback_flow() {
    return *static_cast<MockPasswordSuggestionFlow*>(
        password_autofill_manager_->manual_fallback_flow());
  }

  PasskeyCredential MakeTestPasskeyCredential() {
    return PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                             PasskeyCredential::RpId("example.com"),
                             PasskeyCredential::CredentialId(kPasskeyId),
                             PasskeyCredential::UserId(),
                             PasskeyCredential::Username(kPasskeyNameUtf8));
  }

 protected:
  void SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(bool enable) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    features_.Reset();
    if (enable) {
      features_.InitWithFeatures(
          {features::kPasswordManualFallbackAvailable,
           features::kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu},
          {});
    } else {
      features_.InitWithFeatures(
          {features::kPasswordManualFallbackAvailable},
          {features::kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu});
    }
#endif
  }

  autofill::PasswordFormFillData& fill_data() { return fill_data_; }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;

  std::unique_ptr<MockWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;

  std::u16string test_username_;
  std::u16string test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;

  base::test::ScopedFeatureList features_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PasswordAutofillManagerTest, SuccessfulFillSuggestion) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _));
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 0});
}

TEST_F(PasswordAutofillManagerTest, NoFillingOnNavigation) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  EXPECT_CALL(*client.mock_driver(), FillSuggestion).Times(0);
  password_autofill_manager_->DidNavigateMainFrame();
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 0});
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));

  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _));
  // Accepting a suggestion should trigger a call to hide the popup.
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));
  base::HistogramTester histograms;

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kPassword, 1);
}

// Test that suggestions are filled correctly if account-stored and local
// suggestion have duplicates.
TEST_F(PasswordAutofillManagerTest,
       ExternalDelegateAccountStorePasswordSuggestions) {
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 2u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kAccountStoragePasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[1].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[1].custom_icon),
                     kTestFavicon));

  // When selecting the account-stored credential, make sure the filled
  // password belongs to the selected credential (and not to the first match).
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, duplicate.password_value, _));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kAccountStoragePasswordEntry,
          test_username_),
      SuggestionPosition{.row = 0});
}

// Test that `OnShowPasswordSuggestions` correctly matches the given
// FormFieldData to the known PasswordFormFillData, and extracts the right
// suggestions.
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Gohn Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"John Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Kohn Foo", Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_THAT(open_args.suggestions,
              SuggestionAdditionalLabelsContains(u"foo.com"));
  EXPECT_THAT(open_args.suggestions,
              SuggestionAdditionalLabelsContains(u"foobarrealm.org"));

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT, u"John",
      ShowWebAuthnCredentials(false), ShowIdentityCredentials(false),
      element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(additional.username_value,
                                   Suggestion::Text::IsPrimary(true)),
                  kSeparatorEntry,
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT, u"",
      ShowWebAuthnCredentials(false), ShowIdentityCredentials(false),
      element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Gohn Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"John Foo", Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(u"Kohn Foo", Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  EXPECT_THAT(open_args.suggestions, SuggestionAdditionalLabelsContains(
                                         u"android://com.example2.android/"));
  EXPECT_THAT(open_args.suggestions, SuggestionAdditionalLabelsContains(
                                         u"android://com.example1.android/"));
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      test_username_, ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions);
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);

  // Check that preview of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(std::u16string(), test_password_));
  const Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      autofill::SuggestionType::kPasswordEntry, no_username_string);
  password_autofill_manager_->DidSelectSuggestion(suggestion);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that fill of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(std::u16string(), test_password_, _));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  SuggestionPosition{.row = 0});

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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      test_username_, ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);
  histograms.ExpectUniqueSample(kDropdownShownHistogram,
                                metrics_util::PasswordDropdownState::kStandard,
                                1);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));

  // Clicking at the "Show all passwords row" should trigger a call to open
  // the Password Manager settings page and hide the popup.
  EXPECT_CALL(*client, NavigateToManagePasswordsPage(
                           ManagePasswordsReferrer::kPasswordDropdown));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(
      Suggestion(autofill::SuggestionType::kAllSavedPasswordsEntry),
      SuggestionPosition{.row = 0});

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

  const auto& entries =
      autofill_client.GetUkmRecorder()->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      test_username_, ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
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

  EXPECT_CALL(autofill_client, ShowAutofillSuggestions).Times(0);
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
  histograms.ExpectUniqueSample(
      kDropdownShownHistogram,
      metrics_util::PasswordDropdownState::kStandardGenerate, 1);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre(
                  Suggestion::Icon::kGlobe, Suggestion::Icon::kKey,
                  Suggestion::Icon::kNoIcon, Suggestion::Icon::kSettings));
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(test_username_, Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(generation_string,
                           Suggestion::Text::IsPrimary(true)),
          kSeparatorEntry,
          Suggestion::Text(GetManagePasswordsTitle(),
                           Suggestion::Text::IsPrimary(true))));

  // Click "Generate password".
  EXPECT_CALL(client, GeneratePassword(PasswordGenerationType::kAutomatic));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  password_autofill_manager_->DidAcceptSuggestion(
      Suggestion(autofill::SuggestionType::kGeneratePasswordEntry),
      SuggestionPosition{.row = 1});

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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre(Suggestion::Icon::kKey,
                                       Suggestion::Icon::kNoIcon,
                                       Suggestion::Icon::kSettings));

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(generation_string,
                                   Suggestion::Text::IsPrimary(true)),
                  kSeparatorEntry,
                  Suggestion::Text(GetManagePasswordsTitle(),
                                   Suggestion::Text::IsPrimary(true))));
}

TEST_F(PasswordAutofillManagerTest, DisplayAccountSuggestionsIndicatorIcon) {
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
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions.size(),
              testing::Ge(1u));  // No footer on Android.
  EXPECT_EQ(open_args.trigger_source,
            autofill::AutofillSuggestionTriggerSource::kPasswordManager);
}

TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthNotAvailable) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));

  // Suggestions should always be filled if the authenticator is not available
  // or it cannot be used.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _));

  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(false));
  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});
}

TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthSuccessful) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));

  // The suggestion should be filled if the authentication is successful.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _));

  // Accepting a suggestion should trigger a call to hide the popup.
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  // The authenticator exists and is available.
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});
}

TEST_F(PasswordAutofillManagerTest, DoesntFillSuggestionIfAuthFailed) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));

  // The suggestion should not be filled if the authentication fails.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _))
      .Times(0);
  // Accepting a suggestion should trigger a call to hide the popup.
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  // The authenticator exists and is available.
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});
}

TEST_F(PasswordAutofillManagerTest, CancelsOngoingBiometricAuthOnDestroy) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  EXPECT_CALL(*authenticator_ptr, Cancel());
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnDeleteFillData) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  EXPECT_CALL(*authenticator_ptr, Cancel());
  password_autofill_manager_->DeleteFillData();
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnFillDataChange) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  EXPECT_CALL(*authenticator_ptr, Cancel());
  password_autofill_manager_->OnAddPasswordFillData(CreateTestFormFillData());
}

TEST_F(PasswordAutofillManagerTest, CancelsOngoingBiometricAuthOnNewRequest) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  InitializePasswordAutofillManager(&client, &autofill_client);

  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage);

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  auto authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* authenticator_ptr2 = authenticator2.get();

  // Triggering new authentication should cancel ongoing authentication.
  EXPECT_CALL(*authenticator_ptr, Cancel());
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(*authenticator_ptr2, AuthenticateWithMessage);

  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator2))));

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  // Destroying the manager should cancel ongoing authentication.
  EXPECT_CALL(*authenticator_ptr2, Cancel());
}

TEST_F(PasswordAutofillManagerTest, MetricsRecordedForBiometricAuth) {
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  base::HistogramTester histograms;
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup.
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());

  // The authenticator exists and is available.
  base::OnceCallback<void(bool)> auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(MoveArg<1>(&auth_callback));

  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user.
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry, test_username_),
      SuggestionPosition{.row = 1});

  // Simulate successful authentication and expect successful filling.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_, _));
  std::move(auth_callback).Run(true);

  // Verify reported metrics.
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", true, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime2", kMockElapsedTime,
      1);
}

TEST_F(PasswordAutofillManagerTest,
       ShowsWebAuthnSuggestionsWhenUsePasskeyOnAnotherDeviceInAutofill) {
#if BUILDFLAG(IS_ANDROID)
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/false);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Return a WebAuthn credential.
  auto passkey = MakeTestPasskeyCredential();
  std::vector<PasskeyCredential> passkey_list({passkey});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkey_list)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnCredential,
                  autofill::SuggestionType::kPasswordEntry,
#if !BUILDFLAG(IS_ANDROID)
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice,
#endif  // !BUILDFLAG(IS_ANDROID)
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.suggestions[0].GetPayload<Suggestion::Guid>().value(),
            kPasskeyIdBase64);
  EXPECT_EQ(open_args.suggestions[0].type,
            autofill::SuggestionType::kWebauthnCredential);
  EXPECT_EQ(open_args.suggestions[0].main_text.value, kPasskeyName);
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));
  ASSERT_EQ(open_args.suggestions[0].labels.size(), 1U);
  ASSERT_EQ(open_args.suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(open_args.suggestions[0].labels[0][0].value,
            passkey.GetAuthenticatorLabel());
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that preview of the "username" (i.e. the credential name) works.
  EXPECT_CALL(
      *client.mock_driver(),
      PreviewSuggestion(kPasskeyName, /*password=*/std::u16string(u"")));
  const Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      autofill::SuggestionType::kWebauthnCredential, kPasskeyName,
      autofill::Suggestion::Guid(kPasskeyIdBase64));
  password_autofill_manager_->DidSelectSuggestion(suggestion);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that selecting the credential reports back to the client.
  EXPECT_CALL(*webauthn_credentials_delegate_,
              SelectPasskey(kPasskeyIdBase64, _));
  EXPECT_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion))
      .Times(0);

#if !BUILDFLAG(IS_ANDROID)
  // Check that the button that triggers passkeys from a different devices uses
  // the "Use a *different* passkey" string since passkeys are being offered.
  EXPECT_EQ(
      open_args.suggestions[2].main_text.value,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DIFFERENT_PASSKEY));
#endif  // !BUILDFLAG(IS_ANDROID)

  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(autofill_client, UpdateAutofillSuggestions);
  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  SuggestionPosition{.row = 0});
}

TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSuggestions) {
#if BUILDFLAG(IS_ANDROID)
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Return a WebAuthn credential.
  auto passkey = MakeTestPasskeyCredential();
  std::vector<PasskeyCredential> passkey_list({passkey});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkey_list)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnCredential,
                  autofill::SuggestionType::kPasswordEntry,
#if BUILDFLAG(IS_IOS)
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice,
#endif  // BUILDFLAG(IS_IOS)
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.suggestions[0].GetPayload<Suggestion::Guid>().value(),
            kPasskeyIdBase64);
  EXPECT_EQ(open_args.suggestions[0].type,
            autofill::SuggestionType::kWebauthnCredential);
  EXPECT_EQ(open_args.suggestions[0].main_text.value, kPasskeyName);
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));
  ASSERT_EQ(open_args.suggestions[0].labels.size(), 1U);
  ASSERT_EQ(open_args.suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(open_args.suggestions[0].labels[0][0].value,
            passkey.GetAuthenticatorLabel());
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that preview of the "username" (i.e. the credential name) works.
  EXPECT_CALL(
      *client.mock_driver(),
      PreviewSuggestion(kPasskeyName, /*password=*/std::u16string(u"")));
  const Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      autofill::SuggestionType::kWebauthnCredential, kPasskeyName,
      autofill::Suggestion::Guid(kPasskeyIdBase64));
  password_autofill_manager_->DidSelectSuggestion(suggestion);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that selecting the credential reports back to the client.
  EXPECT_CALL(*webauthn_credentials_delegate_,
              SelectPasskey(kPasskeyIdBase64, _));
  EXPECT_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion))
      .Times(0);

  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(autofill_client, UpdateAutofillSuggestions);
  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  SuggestionPosition{.row = 0});
}

TEST_F(PasswordAutofillManagerTest, ShowsIdentitySuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Return a identity credential.
  std::vector<Suggestion> identity_suggestions;
  std::string id = "user";
  std::string email = "foo@idp.example";
  GURL identity_provider = GURL("https://idp.example/fedcm.json");
  std::string identity_provider_for_display = "idp.example";
  gfx::Image decoded_picture = gfx::Image();

  Suggestion suggestion(base::UTF8ToUTF16(email),
                        autofill::SuggestionType::kIdentityCredential);
  suggestion.labels.push_back({Suggestion::Text(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
      base::UTF8ToUTF16(identity_provider_for_display)))});
  suggestion.custom_icon = decoded_picture;
  auto payload = Suggestion::IdentityCredentialPayload(identity_provider, id);
  suggestion.payload = payload;
  identity_suggestions.push_back(suggestion);

  autofill_client.set_identity_credential_delegate(
      std::make_unique<NiceMock<MockIdentityCredentialDelegate>>());

  ON_CALL(static_cast<MockIdentityCredentialDelegate&>(
              *autofill_client.GetIdentityCredentialDelegate()),
          GetVerifiedAutofillSuggestions)
      .WillByDefault(Return(identity_suggestions));

  // Show password suggestions including identity credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(true), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kIdentityCredential,
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_EQ(open_args.suggestions[0]
                .GetPayload<Suggestion::IdentityCredentialPayload>(),
            payload);
  EXPECT_EQ(open_args.suggestions[0].type,
            autofill::SuggestionType::kIdentityCredential);
  EXPECT_EQ(open_args.suggestions[0].main_text.value, base::UTF8ToUTF16(email));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     decoded_picture));
  ASSERT_EQ(open_args.suggestions[0].labels.size(), 1U);
  ASSERT_EQ(open_args.suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(open_args.suggestions[0].labels[0][0].value,
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_IDENTITY_CREDENTIAL_LABEL_TEXT,
                base::UTF8ToUTF16(identity_provider_for_display)));

  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that selecting the credential reports back to the client.
  EXPECT_CALL(static_cast<MockIdentityCredentialDelegate&>(
                  *autofill_client.GetIdentityCredentialDelegate()),
              NotifySuggestionAccepted);
  // Check that selecting the identity credential enters loading state.
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion))
      .Times(0);

  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(autofill_client, UpdateAutofillSuggestions);
  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  SuggestionPosition{.row = 0});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSignInWithAnotherDevice) {
  SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/false);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Enable WebAuthn autofill.
  std::vector<PasskeyCredential> passkeys;
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkeys)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));

  // Check that the button shows the correct text.
  EXPECT_EQ(open_args.suggestions[1].main_text.value,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_PASSKEY));
}

TEST_F(PasswordAutofillManagerTest, DoesntShowWebAuthnSignInWithAnotherDevice) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Enable WebAuthn autofill.
  std::vector<PasskeyCredential> passkeys;
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkeys)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(false));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
}

// Regression test for crbug.com/1370037.
TEST_F(PasswordAutofillManagerTest,
       WebAuthnFaviconWithoutPasswordsWhenUseAnotherDeviceInAutofill) {
  SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/false);
  // Initialize a PasswordAutofillManager with an empty password form.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  password_autofill_manager_->DeleteFillData();
  autofill::PasswordFormFillData data;
  data.url = GURL(kMainFrameUrl);
  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
      .WillOnce(Invoke(RespondWithTestIcon));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Enable WebAuthn autofill to return a credential.
  std::vector<PasskeyCredential> passkeys({MakeTestPasskeyCredential()});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkeys)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  // Show webauthn suggestions with the correct favicon.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnCredential,
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));
}

// Regression test for crbug.com/1370037.
TEST_F(PasswordAutofillManagerTest, WebAuthnFaviconWithoutPasswords) {
  // Initialize a PasswordAutofillManager with an empty password form.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  password_autofill_manager_->DeleteFillData();
  autofill::PasswordFormFillData data;
  data.url = GURL(kMainFrameUrl);
  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
      .WillOnce(Invoke(RespondWithTestIcon));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Enable WebAuthn autofill to return a credential.
  std::vector<PasskeyCredential> passkeys({MakeTestPasskeyCredential()});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkeys)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  // Show webauthn suggestions with the correct favicon.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnCredential,
#if BUILDFLAG(IS_IOS)
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice,
#endif  // BUILDFLAG(IS_IOS)
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
  EXPECT_TRUE(
      std::holds_alternative<gfx::Image>(open_args.suggestions[0].custom_icon));
  EXPECT_TRUE(
      AreImagesEqual(std::get<gfx::Image>(open_args.suggestions[0].custom_icon),
                     kTestFavicon));
}

// Regression test for crbug.com/1362742.
TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSignInWithoutPasswordData) {
  base::test::ScopedFeatureList features;
  SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/false);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // InitializePasswordAutofillManager sets fill data, delete it to simulate
  // having no data.
  password_autofill_manager_->DeleteFillData();

  // Enable WebAuthn autofill.
  std::vector<PasskeyCredential> passkeys;
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkeys)));
  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(true));

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnSignInWithAnotherDevice));

  // Check that the button shows the correct text.
  EXPECT_EQ(open_args.suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_PASSKEY));
}

TEST_F(PasswordAutofillManagerTest, WebAuthnSignInLaunchesWebAuthnFlow) {
  SetUseAPasskeyOnAnotherDeviceFeatureOnDesktop(/*enable=*/false);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Check that selecting the button reports back to the client.
  EXPECT_CALL(*webauthn_credentials_delegate_, LaunchSecurityKeyOrHybridFlow());
  EXPECT_CALL(autofill_client,
              HideAutofillSuggestions(
                  autofill::SuggestionHidingReason::kAcceptSuggestion));

  Suggestion suggestion;
  suggestion.main_text.value =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_PASSKEY);
  suggestion.type = autofill::SuggestionType::kWebauthnSignInWithAnotherDevice;
  suggestion.payload = autofill::Suggestion::Payload();
  password_autofill_manager_->DidAcceptSuggestion(suggestion,
                                                  SuggestionPosition{.row = 0});
}

// Test that the AutofillSuggestionAvailability is set according to the popup
// availability.
TEST_F(PasswordAutofillManagerTest,
       AutofillSuggestionAvailabilityNoSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // No popup, thus kNoSuggestions is set on the field.
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(false));
  EXPECT_CALL(
      *client.mock_driver(),
      SetSuggestionAvailability(
          kElementId,
          autofill::mojom::AutofillSuggestionAvailability::kNoSuggestions));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
}

// Test that the AutofillSuggestionAvailability is set according to the popup
// availability.
TEST_F(PasswordAutofillManagerTest,
       AutofillSuggestionAvailabilityAutofillAvailable) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // The popup is visible, kAutofillAvailable is set.
  EXPECT_CALL(*client.mock_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_CALL(
      *client.mock_driver(),
      SetSuggestionAvailability(
          kElementId,
          autofill::mojom::AutofillSuggestionAvailability::kAutofillAvailable));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      std::u16string(), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false), gfx::RectF());
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordAutofillManagerTest, NoPreviewSuggestionWithAuthBeforeFilling) {
  TestPasswordManagerClient client;
  ON_CALL(*client.GetPasswordFeatureManager(),
          IsBiometricAuthenticationBeforeFillingEnabled)
      .WillByDefault(Return(true));

  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(), PreviewSuggestion).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());
}

TEST_F(PasswordAutofillManagerTest, ManualFallback_InvokesFlow) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client,
                                    /*autofill_client=*/nullptr);
  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(manual_fallback_flow(),
              RunFlow(kElementId, bounds, base::i18n::LEFT_TO_RIGHT));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId,
      autofill::AutofillSuggestionTriggerSource::kManualFallbackPasswords,
      base::i18n::LEFT_TO_RIGHT, std::u16string(),
      ShowWebAuthnCredentials(false), ShowIdentityCredentials(false), bounds);
}

TEST_F(PasswordAutofillManagerTest, ManualFallback_FlowResetOnNavigation) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client,
                                    /*autofill_client=*/nullptr);
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId,
      autofill::AutofillSuggestionTriggerSource::kManualFallbackPasswords,
      base::i18n::LEFT_TO_RIGHT, std::u16string(),
      ShowWebAuthnCredentials(false), ShowIdentityCredentials(false),
      gfx::RectF(1, 1, 2, 2));
  EXPECT_TRUE(password_autofill_manager_->manual_fallback_flow());
  password_autofill_manager_->DidNavigateMainFrame();

  EXPECT_FALSE(password_autofill_manager_->manual_fallback_flow());
}

TEST_F(PasswordAutofillManagerTest,
       WebAuthnCredentialSuggestionsPersistLoadingStateUntilHidePopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  std::vector<PasskeyCredential> passkey_list({MakeTestPasskeyCredential()});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkey_list)));

  // Show suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/u"", ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), gfx::RectF());

  // Select a passkey.
  std::vector<autofill::Suggestion> updatedSuggestions;
  WebAuthnCredentialsDelegate::OnPasskeySelectedCallback hide_callback;
  EXPECT_CALL(autofill_client, UpdateAutofillSuggestions)
      .WillOnce(testing::SaveArg<0>(&updatedSuggestions));
  EXPECT_CALL(*webauthn_credentials_delegate_, SelectPasskey)
      .WillRepeatedly(MoveArg<1>(std::move(&hide_callback)));
  EXPECT_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_client, HideAutofillSuggestions(_)).Times(0);
  password_autofill_manager_->DidAcceptSuggestion(open_args.suggestions[0],
                                                  SuggestionPosition{.row = 0});

  // Since a passkey is selected, the popup will be updated:
  EXPECT_TRUE(updatedSuggestions[0].is_loading);
  EXPECT_FALSE(updatedSuggestions[0].IsAcceptable());
  EXPECT_TRUE(updatedSuggestions[1].HasDeactivatedStyle());
  EXPECT_FALSE(updatedSuggestions[1].IsAcceptable());

  // Show suggestions again.
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  EXPECT_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
      .WillOnce(Return(true));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/u"", ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), gfx::RectF());

  // Verify that the loading state persisted.
  EXPECT_TRUE(open_args.suggestions[0].is_loading);
  EXPECT_FALSE(open_args.suggestions[0].IsAcceptable());
  EXPECT_TRUE(open_args.suggestions[1].HasDeactivatedStyle());
  EXPECT_FALSE(open_args.suggestions[1].IsAcceptable());

  // After calling hide callback the loading state is not persisted anymore:
  EXPECT_CALL(autofill_client, HideAutofillSuggestions);
  std::move(hide_callback).Run();

  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));
  EXPECT_CALL(*webauthn_credentials_delegate_, HasPendingPasskeySelection)
      .WillOnce(Return(false));
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/u"", ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), gfx::RectF());

  EXPECT_FALSE(open_args.suggestions[0].is_loading);
  EXPECT_TRUE(open_args.suggestions[0].IsAcceptable());
  EXPECT_FALSE(open_args.suggestions[1].HasDeactivatedStyle());
  EXPECT_TRUE(open_args.suggestions[1].IsAcceptable());
}

TEST_F(PasswordAutofillManagerTest,
       ManualFallback_NoPasswordForm_MetricsNotEmittedOnNavigation) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client,
                                    /*autofill_client=*/nullptr);

  base::HistogramTester histograms;
  password_autofill_manager_->DidNavigateMainFrame();

  EXPECT_THAT(histograms.GetTotalCountsForPrefix("Autofill.Funnel."),
              ::testing::IsEmpty());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
TEST_F(PasswordAutofillManagerTest, ShowCrossDomainConfirmationPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  autofill::PasswordFormFillData cross_domain_fill_data =
      CreateTestFormFillData();
  cross_domain_fill_data.preferred_login.is_grouped_affiliation = true;
  password_autofill_manager_->OnAddPasswordFillData(cross_domain_fill_data);
  EXPECT_CALL(client, ShowCrossDomainConfirmationPopup);

  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry,
          cross_domain_fill_data.preferred_login.username_value),
      SuggestionPosition{.row = 0});
}

TEST_F(PasswordAutofillManagerTest, EmitUMAIfAtLeastOneGroupedCredential) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  autofill::PasswordFormFillData cross_domain_fill_data =
      CreateTestFormFillData();
  autofill::PasswordAndMetadata grouped_match_credential;
  grouped_match_credential.username_value = u"";
  grouped_match_credential.password_value = u"";
  grouped_match_credential.realm = "http://grouped-realm.com";
  grouped_match_credential.is_grouped_affiliation = true;
  cross_domain_fill_data.additional_logins.push_back(grouped_match_credential);

  password_autofill_manager_->OnAddPasswordFillData(cross_domain_fill_data);

  base::HistogramTester histograms;
  password_autofill_manager_->DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          autofill::SuggestionType::kPasswordEntry,
          cross_domain_fill_data.preferred_login.username_value),
      SuggestionPosition{.row = 0});
  histograms.ExpectUniqueSample(
      "PasswordManager.FillSuggestionsGroupedMatchAccepted", /*sample=*/false,
      /*expected_bucket_count=*/1);
}
#endif

TEST_F(PasswordAutofillManagerTest, WaitForPasskeysWithAutofocusTrigger) {
  base::test::ScopedFeatureList wait_for_passkey_feature_list_(
      features::kDelaySuggestionsOnAutofocusWaitingForPasskeys);
#if BUILDFLAG(IS_ANDROID)
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(
          base::unexpected(WebAuthnCredentialsDelegate::
                               PasskeysUnavailableReason::kNotReceived)));
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions).Times(0);

  // This call will not attempt to show suggestions because passkeys are still
  // pending.
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId,
      autofill::AutofillSuggestionTriggerSource::
          kPasswordManagerProcessedFocusedField,
      base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);

  // Simulate a WebAuthn credential being provided by the platform via the
  // WebAuthnCredentialsDelegate.
  std::vector<PasskeyCredential> passkey_list({MakeTestPasskeyCredential()});
  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(base::ok(&passkey_list)));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  task_environment().FastForwardUntilNoTasksRemain();

  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kWebauthnCredential,
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
}

TEST_F(PasswordAutofillManagerTest,
       WaitForPasskeysOverriddenByNonAutofocusTrigger) {
  base::test::ScopedFeatureList wait_for_passkey_feature_list_(
      features::kDelaySuggestionsOnAutofocusWaitingForPasskeys);
#if BUILDFLAG(IS_ANDROID)
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(
          base::unexpected(WebAuthnCredentialsDelegate::
                               PasskeysUnavailableReason::kNotReceived)));
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions).Times(0);

  // This call will not attempt to show suggestions because passkeys are still
  // pending.
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId,
      autofill::AutofillSuggestionTriggerSource::
          kPasswordManagerProcessedFocusedField,
      base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);

  // Show password suggestions without any passkeys.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  // Called again with different trigger.
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId, kDefaultTriggerSource, base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);

  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
}

TEST_F(PasswordAutofillManagerTest,
       WaitForPasskeysIgnoredWhenWebAuthnRequestAborted) {
  base::test::ScopedFeatureList wait_for_passkey_feature_list_(
      features::kDelaySuggestionsOnAutofocusWaitingForPasskeys);
#if BUILDFLAG(IS_ANDROID)
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  EXPECT_CALL(*webauthn_credentials_delegate_,
              IsSecurityKeyOrHybridFlowAvailable)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*webauthn_credentials_delegate_, GetPasskeys)
      .WillRepeatedly(Return(
          base::unexpected(WebAuthnCredentialsDelegate::
                               PasskeysUnavailableReason::kRequestAborted)));
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillSuggestions)
      .WillOnce(SavePopupOpenArgs(open_args));

  // This should show suggestions without passkeys because the request was
  // aborted.
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      kElementId,
      autofill::AutofillSuggestionTriggerSource::
          kPasswordManagerProcessedFocusedField,
      base::i18n::RIGHT_TO_LEFT,
      /*typed_username=*/std::u16string(), ShowWebAuthnCredentials(true),
      ShowIdentityCredentials(false), element_bounds);

  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  autofill::SuggestionType::kPasswordEntry,
                  autofill::SuggestionType::kSeparator,
                  autofill::SuggestionType::kAllSavedPasswordsEntry));
}

}  // namespace
}  // namespace password_manager
