// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include <vector>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::EqualsSuggestion;
using autofill::PasswordAndMetadata;
using autofill::PasswordFormFillData;
using autofill::Suggestion;
using autofill::SuggestionType;
using testing::_;
using testing::AllOf;
using testing::Conditional;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::ReturnRef;

Matcher<Suggestion> EqualsDomainPasswordSuggestion(
    SuggestionType id,
    const std::u16string& main_text,
    const std::u16string& password_label,
    const std::u16string& realm_label,
    const gfx::Image& custom_icon,
    Suggestion::Icon trailing_icon = Suggestion::Icon::kNoIcon) {
  const std::u16string voice_over_suffix =
      realm_label.empty() ? u"" : base::StrCat({u", ", realm_label});
  return AllOf(
      EqualsSuggestion(id, main_text, Suggestion::Icon::kGlobe),
      Field("additional_label", &Suggestion::additional_label, realm_label),
      Field("labels", &Suggestion::labels,
            ElementsAre(ElementsAre(Suggestion::Text(password_label)))),
      Field("voice_over", &Suggestion::voice_over,
            Conditional(
                realm_label.empty(),
                l10n_util::GetStringFUTF16(
                    IDS_PASSWORD_MANAGER_PASSWORD_FOR_ACCOUNT, main_text),
                base::StrCat(
                    {l10n_util::GetStringFUTF16(
                         IDS_PASSWORD_MANAGER_PASSWORD_FOR_ACCOUNT, main_text),
                     u", ", realm_label}))),
      Field("custom_icon", &Suggestion::custom_icon, custom_icon),
      Field("trailing_icon", &Suggestion::trailing_icon, trailing_icon));
}

Matcher<Suggestion> EqualsPasskeySuggestion(
    const std::u16string& main_text,
    const std::u16string& authenticator_label,
    const gfx::Image& custom_icon,
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(SuggestionType::kWebauthnCredential, main_text,
                       Suggestion::Icon::kGlobe),
      Field("labels", &Suggestion::labels,
            ElementsAre(ElementsAre(Suggestion::Text(authenticator_label)))),
      Field("custom_icon", &Suggestion::custom_icon, custom_icon),
      Field("payload", &Suggestion::payload, payload));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
Matcher<Suggestion> EqualsManualFallbackSuggestion(
    SuggestionType id,
    const std::u16string& main_text,
    const std::u16string& username_label,
    Suggestion::Icon icon,
    bool is_acceptable,
    absl::variant<gfx::Image,
                  Suggestion::CustomIconUrl,
                  Suggestion::FaviconDetails> custom_icon,
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(id, main_text, icon),
      Field(
          "labels", &Suggestion::labels,
          ElementsAre(ElementsAre(autofill::Suggestion::Text(username_label)))),
      Field("is_acceptable", &Suggestion::is_acceptable, is_acceptable),
      Field("custom_icon", &Suggestion::custom_icon, custom_icon),
      Field("payload", &Suggestion::payload, payload));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

Matcher<Suggestion> EqualsGeneratePasswordSuggestion() {
  return EqualsSuggestion(
      SuggestionType::kGeneratePasswordEntry,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD),
      Suggestion::Icon::kKey);
}

Matcher<Suggestion> EqualsOptInToAccountThenGeneratePasswordSuggestion() {
  return EqualsSuggestion(
      SuggestionType::kPasswordAccountStorageOptInAndGenerate,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD),
      Suggestion::Icon::kKey);
}

Matcher<Suggestion> EqualsEntryToOptInToAccountStorageThenFill(
    bool has_passkey_sync) {
  return EqualsSuggestion(
      SuggestionType::kPasswordAccountStorageOptIn,
      has_passkey_sync
          ? l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE_WITH_PASSKEYS)
          : l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE),
      Suggestion::Icon::kGoogle);
}

Matcher<Suggestion> EqualsAccountStorageResignin() {
  return EqualsSuggestion(
      SuggestionType::kPasswordAccountStorageReSignin,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_RE_SIGNIN_ACCOUNT_STORE),
      Suggestion::Icon::kGoogle);
}

Matcher<Suggestion> EqualsManagePasswordsSuggestion(
    bool has_webauthn_credential = false) {
  return AllOf(EqualsSuggestion(
                   SuggestionType::kAllSavedPasswordsEntry,
                   has_webauthn_credential
                       ? l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS)
                       : l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
                   Suggestion::Icon::kSettings),
               Field("trailing_icon", &Suggestion::trailing_icon,
                     Suggestion::Icon::kGooglePasswordManager));
}

MATCHER_P(SuggestionHasFaviconDetails, favicon_details, "") {
  const auto* arg_favicon_details =
      absl::get_if<Suggestion::FaviconDetails>(&arg.custom_icon);
  return arg_favicon_details && *arg_favicon_details == favicon_details;
}

MATCHER(FaviconCanBeRequestedFromGoogle, "") {
  const auto* arg_favicon_details =
      absl::get_if<Suggestion::FaviconDetails>(&arg.custom_icon);
  EXPECT_TRUE(!!arg_favicon_details);
  return arg_favicon_details &&
         arg_favicon_details->can_be_requested_from_google;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(const syncer::SyncService*, GetSyncService, (), (const override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));

  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override {
    return &feature_manager_;
  }

  MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return &feature_manager_;
  }

 private:
  NiceMock<MockPasswordFeatureManager> feature_manager_;
};

}  // namespace

class PasswordSuggestionGeneratorTest : public testing::Test {
 public:
  PasswordSuggestionGeneratorTest() : generator_(&driver(), &client()) {
    ON_CALL(client_, GetSyncService).WillByDefault(Return(&sync_service()));
    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&credentials_delegate()));
  }

  const gfx::Image& favicon() const { return favicon_; }

  StubPasswordManagerDriver& driver() { return driver_; }

  syncer::MockSyncService& sync_service() { return mock_sync_service_; }

  MockPasswordManagerClient& client() { return client_; }

  MockWebAuthnCredentialsDelegate& credentials_delegate() {
    return credentials_delegate_;
  }

  PasswordSuggestionGenerator& generator() { return generator_; }

  PasswordForm password_form() const {
    return CreateEntry("username@example.com", "password",
                       GURL("https://google.com/"),
                       PasswordForm::MatchType::kExact);
  }

  PasswordForm password_form_no_username() const {
    return CreateEntry("", "password", GURL("https://google.com/"),
                       PasswordForm::MatchType::kExact);
  }

  CredentialUIEntry credential_ui_entry() const {
    return CredentialUIEntry(password_form());
  }

  CredentialUIEntry credential_ui_entry_no_username() const {
    return CredentialUIEntry(password_form_no_username());
  }

  PasswordFormFillData password_form_fill_data() const {
    PasswordFormFillData fill_data;
    fill_data.preferred_login.username_value = u"username";
    fill_data.preferred_login.password_value = u"password";
    fill_data.preferred_login.realm = "";
    fill_data.preferred_login.uses_account_store = false;
    return fill_data;
  }

  PasskeyCredential passkey_credential(PasskeyCredential::Source source,
                                       const std::string& username) const {
    return PasskeyCredential(
        source, PasskeyCredential::RpId("gensokyo.com"),
        PasskeyCredential::CredentialId(
            std::vector<uint8_t>{'a', 'b', 'c', 'd'}),
        PasskeyCredential::UserId(std::vector<uint8_t>{'i', 'j', 'k'}),
        PasskeyCredential::Username(username),
        PasskeyCredential::DisplayName("Display name"));
  }

  std::u16string password_label(size_t length) const {
    return std::u16string(length, constants::kPasswordReplacementChar);
  }

  CredentialUIEntry android_credential_ui_entry() const {
    PasswordForm form;
    form.username_value = u"username@example.com";
    form.password_value = u"password";
    const std::string url =
        "android://"
        "Jzj5T2E45Hb33D-lk-"
        "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg=="
        "@com.netflix.mediaclient/";
    form.url = GURL(url);
    form.signon_realm = url;
    // This field is populated for Android credentials.
    form.app_display_name = "Netflix";
    return CredentialUIEntry(form);
  }

  std::vector<Suggestion> GenerateSuggestedPasswordsSection(
      const std::vector<PasswordForm> suggested_credentials,
      IsTriggeredOnPasswordForm on_password_form) {
    return generator().GetManualFallbackSuggestions(
        base::make_span(suggested_credentials),
        base::span<const CredentialUIEntry>(), on_password_form);
  }

  std::vector<Suggestion> GenerateAllPasswordsSection(
      const std::vector<CredentialUIEntry>& all_credentials,
      IsTriggeredOnPasswordForm on_password_form) {
    return generator().GetManualFallbackSuggestions(
        base::span<const PasswordForm>(), base::make_span(all_credentials),
        on_password_form);
  }

  std::vector<Suggestion> GenerateBothSections(
      const std::vector<PasswordForm> suggested_credentials,
      const std::vector<CredentialUIEntry>& all_credentials,
      IsTriggeredOnPasswordForm on_password_form) {
    return generator().GetManualFallbackSuggestions(
        base::make_span(suggested_credentials),
        base::make_span(all_credentials), on_password_form);
  }

  void EnablePasswordSync() {
    ON_CALL(sync_service(), GetActiveDataTypes)
        .WillByDefault(Return(syncer::DataTypeSet({syncer::PASSWORDS})));
    ON_CALL(sync_service(), HasSyncConsent).WillByDefault(Return(true));
    ON_CALL(*sync_service().GetMockUserSettings(), GetSelectedTypes)
        .WillByDefault(Return(syncer::UserSelectableTypeSet(
            {syncer::UserSelectableType::kPasswords})));
    ON_CALL(*sync_service().GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete)
        .WillByDefault(Return(true));
  }

 private:
  gfx::Image favicon_;

  NiceMock<syncer::MockSyncService> mock_sync_service_;
  NiceMock<MockPasswordManagerClient> client_;
  NiceMock<MockWebAuthnCredentialsDelegate> credentials_delegate_;
  StubPasswordManagerDriver driver_;
  PasswordSuggestionGenerator generator_;
};

// Test that no suggestions are generated from an empty `PasswordFormFillData`.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_NoPasswordFormFillData) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions, IsEmpty());
}

// Verify that password suggestions are not generated when
// `ShowPasswordSuggestions` is `false`.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_DontShowPasswordSuggestions) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(false),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions, IsEmpty());
}

// Verify the suggestion content for the profile store credential.
TEST_F(PasswordSuggestionGeneratorTest, PasswordSuggestions_FromProfileStore) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verify that the trailing icon is not set for the account store credential.
TEST_F(PasswordSuggestionGeneratorTest, PasswordSuggestions_FromAccountStore) {
  PasswordFormFillData fill_data = password_form_fill_data();
  fill_data.preferred_login.uses_account_store = true;

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      fill_data, favicon(), /*username_filter=*/u"", OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kAccountStoragePasswordEntry,
                              u"username", password_label(8u),
                              /*realm_label=*/u"", favicon(),
                              Suggestion::Icon::kNoIcon),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Verify the realm label for the credential saved on a different domain.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_WithDifferentRealm) {
  PasswordFormFillData fill_data = password_form_fill_data();
  fill_data.preferred_login.realm = "https://example.com/login/data.html";

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      fill_data, favicon(), /*username_filter=*/u"", OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"example.com", favicon()),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Verify the suggestion content for the additional login.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_WithAdditionalLogin) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{syncer::kSyncWebauthnCredentials, false}});
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  PasswordFormFillData fill_data = password_form_fill_data();
  PasswordAndMetadata additional_login;
  additional_login.username_value = u"additional_login";
  additional_login.password_value = u"additional_password";
  additional_login.realm = "https://additional.login.com";
  additional_login.uses_account_store = true;
  fill_data.additional_logins.emplace_back(std::move(additional_login));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      fill_data, favicon(), /*username_filter=*/u"", OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsDomainPasswordSuggestion(
                              SuggestionType::kAccountStoragePasswordEntry,
                              u"additional_login", password_label(19u),
                              /*realm_label=*/u"additional.login.com",
                              favicon(), Suggestion::Icon::kNoIcon),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Verify that suggestions for additional logins are sorted by username.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_MultipleAdditionalLogins) {
  PasswordFormFillData fill_data = password_form_fill_data();
  PasswordAndMetadata foo_login;
  foo_login.username_value = u"foo";
  foo_login.password_value = u"foo_password";
  fill_data.additional_logins.emplace_back(std::move(foo_login));
  PasswordAndMetadata bar_login;
  bar_login.username_value = u"bar";
  bar_login.password_value = u"bar_password";
  fill_data.additional_logins.emplace_back(std::move(bar_login));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      fill_data, favicon(), /*username_filter=*/u"", OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry,
                                         u"username", password_label(8u),
                                         /*realm_label=*/u"", favicon()),
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry, u"bar",
                                         password_label(12u),
                                         /*realm_label=*/u"", favicon()),
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry, u"foo",
                                         password_label(12u),
                                         /*realm_label=*/u"", favicon()),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion()));
}

// Verify that no passkeys suggestions are generated when
//  `ShowWebAuthnCredentials` is `true`, but there're not passkeys saved.
TEST_F(PasswordSuggestionGeneratorTest, PasskeySuggestions_NoPasskeysSaved) {
  std::optional<std::vector<PasskeyCredential>> passkeys = std::nullopt;
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(true));

  EXPECT_THAT(suggestions, IsEmpty());
}

// Verify that no passkeys suggestions are generated when there're passkeys
// saved but `ShowWebAuthnCredentials` is `false`.
TEST_F(PasswordSuggestionGeneratorTest, PasskeySuggestions_DontShowPasskey) {
  const auto passkeys =
      std::optional(std::vector<PasskeyCredential>{passkey_credential(
          PasskeyCredential::Source::kWindowsHello, "username")});
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions, IsEmpty());
}

// Verify the passkey suggestion content.
TEST_F(PasswordSuggestionGeneratorTest, PasskeySuggestions_SingleSavedPasskey) {
  const auto passkey =
      passkey_credential(PasskeyCredential::Source::kWindowsHello, "username");
  const auto passkeys = std::optional(std::vector<PasskeyCredential>{passkey});
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsPasskeySuggestion(
              u"username",
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_PASSKEY_FROM_WINDOWS_HELLO),
              favicon(),
              Suggestion::Guid(base::Base64Encode(passkey.credential_id()))),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion(
              /*has_webauthn_credential=*/true)));
}

// Verify that passkey suggestions are not sorted by username.
TEST_F(PasswordSuggestionGeneratorTest,
       PasskeySuggestions_MultipleSavedPasskeys) {
  const auto foo_passkey =
      passkey_credential(PasskeyCredential::Source::kTouchId, "foo");
  const auto bar_passkey =
      passkey_credential(PasskeyCredential::Source::kICloudKeychain, "bar");
  const auto passkeys =
      std::optional(std::vector<PasskeyCredential>{foo_passkey, bar_passkey});
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsPasskeySuggestion(
                      u"foo",
                      l10n_util::GetStringUTF16(
                          IDS_PASSWORD_MANAGER_PASSKEY_FROM_CHROME_PROFILE),
                      favicon(),
                      Suggestion::Guid(
                          base::Base64Encode(foo_passkey.credential_id()))),
                  EqualsPasskeySuggestion(
                      u"bar",
                      l10n_util::GetStringUTF16(
                          IDS_PASSWORD_MANAGER_PASSKEY_FROM_ICLOUD_KEYCHAIN),
                      favicon(),
                      Suggestion::Guid(
                          base::Base64Encode(bar_passkey.credential_id()))),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsManagePasswordsSuggestion(
                      /*has_webauthn_credential=*/true)));
}

// Test that the password generation suggestion is not added if there're no
// saved credentials for the current domain.
TEST_F(PasswordSuggestionGeneratorTest, GeneratePassword_NoCredentials) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(true), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));
  EXPECT_THAT(suggestions, IsEmpty());
}

// Test that the password generation suggestion is added when the user has a
// saved password for the current domain.
TEST_F(PasswordSuggestionGeneratorTest, GeneratePassword_HasSavedPassword) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(true), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsGeneratePasswordSuggestion(),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Test that the password generation suggestion is added when the user has a
// saved passkey for the current domain.
TEST_F(PasswordSuggestionGeneratorTest, GeneratePassword_HasSavedPasskey) {
  const auto passkey =
      passkey_credential(PasskeyCredential::Source::kWindowsHello, "username");
  const auto passkeys = std::optional(std::vector<PasskeyCredential>{passkey});
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(true), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsPasskeySuggestion(
              u"username",
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_PASSKEY_FROM_WINDOWS_HELLO),
              favicon(),
              Suggestion::Guid(base::Base64Encode(passkey.credential_id()))),
          EqualsGeneratePasswordSuggestion(),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion(
              /*has_webauthn_credential=*/true)));
}

// Verifies the generate password suggestion content when account storage opt in
// should be shown to the user.
TEST_F(PasswordSuggestionGeneratorTest,
       GeneratePassword_ShouldShowAccountStorageOptIn) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncWebauthnCredentials);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ON_CALL(*client().GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(true), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsOptInToAccountThenGeneratePasswordSuggestion(),
                          EqualsEntryToOptInToAccountStorageThenFill(
                              /*has_passkey_sync=*/false),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Verifies that opt into account storage suggestion is still shown if there're
// no saved credentials for the current domain.
TEST_F(PasswordSuggestionGeneratorTest,
       OptInToAccountStorage_NoSavedCredentials) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncWebauthnCredentials);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ON_CALL(*client().GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsEntryToOptInToAccountStorageThenFill(
                  /*has_passkey_sync=*/false)));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verifies the opt into account storage suggestion content when the
// `kSyncWebauthnCredentials` feature is enabled.
TEST_F(PasswordSuggestionGeneratorTest, OptInToAccountStorage_HasPasskeySync) {
  base::test::ScopedFeatureList feature_list(syncer::kSyncWebauthnCredentials);
  ON_CALL(*client().GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsEntryToOptInToAccountStorageThenFill(
                              /*has_passkey_sync=*/true),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Test that account storage resignin suggestion is still shown if the there're
// no credentials saved for the current domain.
TEST_F(PasswordSuggestionGeneratorTest,
       AccountStorageResignin_NoSavedCredentials) {
  ON_CALL(*client().GetPasswordFeatureManager(),
          ShouldShowAccountStorageReSignin)
      .WillByDefault(Return(true));
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      /*fill_data=*/{}, favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions, ElementsAre(EqualsAccountStorageResignin()));
}

// Test the account storage resignin suggestion content.
TEST_F(PasswordSuggestionGeneratorTest,
       AccountStorageResignin_HasSavedPassword) {
  ON_CALL(*client().GetPasswordFeatureManager(),
          ShouldShowAccountStorageReSignin)
      .WillByDefault(Return(true));
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      password_form_fill_data(), favicon(), /*username_filter=*/u"",
      OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsDomainPasswordSuggestion(
                              SuggestionType::kPasswordEntry, u"username",
                              password_label(8u),
                              /*realm_label=*/u"", favicon()),
                          EqualsAccountStorageResignin(),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Test the suggestion order when all possible suggestions should be generated.
TEST_F(PasswordSuggestionGeneratorTest, DomainSuggestions_SuggestionOrder) {
  // Configure saved password data.
  PasswordFormFillData fill_data = password_form_fill_data();
  PasswordAndMetadata foo_login;
  foo_login.username_value = u"foo";
  foo_login.password_value = u"foo_password";
  fill_data.additional_logins.emplace_back(std::move(foo_login));
  PasswordAndMetadata bar_login;
  bar_login.username_value = u"bar";
  bar_login.password_value = u"bar_password";
  fill_data.additional_logins.emplace_back(std::move(bar_login));

  // Configure saved passkeys.
  const auto foo_passkey =
      passkey_credential(PasskeyCredential::Source::kTouchId, "foo");
  const auto bar_passkey =
      passkey_credential(PasskeyCredential::Source::kICloudKeychain, "bar");
  const auto passkeys =
      std::optional(std::vector<PasskeyCredential>{foo_passkey, bar_passkey});
  ON_CALL(credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(passkeys));

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncWebauthnCredentials);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ON_CALL(*client().GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn)
      .WillByDefault(Return(true));
  ON_CALL(*client().GetPasswordFeatureManager(),
          ShouldShowAccountStorageReSignin)
      .WillByDefault(Return(true));

  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      fill_data, favicon(), /*username_filter=*/u"", OffersGeneration(true),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsPasskeySuggestion(
              u"foo",
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_PASSKEY_FROM_CHROME_PROFILE),
              favicon(),
              Suggestion::Guid(
                  base::Base64Encode(foo_passkey.credential_id()))),
          EqualsPasskeySuggestion(
              u"bar",
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_PASSKEY_FROM_ICLOUD_KEYCHAIN),
              favicon(),
              Suggestion::Guid(
                  base::Base64Encode(bar_passkey.credential_id()))),
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry,
                                         u"username", password_label(8u),
                                         /*realm_label=*/u"", favicon()),
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry, u"bar",
                                         password_label(12u),
                                         /*realm_label=*/u"", favicon()),
          EqualsDomainPasswordSuggestion(SuggestionType::kPasswordEntry, u"foo",
                                         password_label(12u),
                                         /*realm_label=*/u"", favicon()),
          EqualsOptInToAccountThenGeneratePasswordSuggestion(),
          EqualsEntryToOptInToAccountStorageThenFill(
              /*has_passkey_sync=*/false),
          EqualsAccountStorageResignin(),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion(
              /*has_webauthn_credential=*/true)));
}

// Manual fallback suggestions are only relevant for desktop platform.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PasswordSuggestionGeneratorTest, ManualFallback_NoCredentials) {
  std::vector<Suggestion> suggestions = GenerateBothSections(
      std::vector<PasswordForm>(), std::vector<CredentialUIEntry>(),
      IsTriggeredOnPasswordForm(true));
  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"username@example.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"username@example.com", u"password",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/false)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"username@example.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"username@example.com", u"password",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_AndroidCredential_SuggestionContent) {
  CredentialUIEntry credential = android_credential_ui_entry();
  std::string sign_on = credential.GetFirstSignonRealm();
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"Netflix",
                              u"username@example.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              /*custom_icon=*/gfx::Image(),
                              Suggestion::PasswordSuggestionDetails(
                                  u"username@example.com", u"password", sign_on,
                                  u"Netflix", /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_NoUsername_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form_no_username()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN),
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://google.com")),
              Suggestion::PasswordSuggestionDetails(
                  u"", u"password", "https://google.com/", u"google.com",
                  /*is_cross_domain=*/false)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_NoUsername_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry_no_username()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN),
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://google.com")),
              Suggestion::PasswordSuggestionDetails(
                  u"", u"password", "https://google.com/", u"google.com",
                  /*is_cross_domain=*/true)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_NonPasswordForm_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form()}, IsTriggeredOnPasswordForm(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"username@example.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/false,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"username@example.com", u"password",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/false)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_NonPasswordForm_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry()}, IsTriggeredOnPasswordForm(false));

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"username@example.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/false,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"username@example.com", u"password",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_AllDomainsAreUsed) {
  PasswordForm form_1 =
      CreateEntry("example@google.com", "password", GURL("https://google.com/"),
                  PasswordForm::MatchType::kExact);
  PasswordForm form_2 =
      CreateEntry("example@google.com", "password", GURL("https://amazon.com/"),
                  PasswordForm::MatchType::kExact);
  CredentialUIEntry entry({std::move(form_1), std::move(form_2)});
  std::vector<Suggestion> suggestions =
      GenerateAllPasswordsSection({entry}, IsTriggeredOnPasswordForm(true));

  // Only the first domain is used to create the suggestion.
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"amazon.com",
                              u"example@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://amazon.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"example@google.com", u"password",
                                  "https://amazon.com/", u"amazon.com",
                                  /*is_cross_domain=*/true)),
                          EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"example@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"example@google.com", u"password",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_SortedByDomain) {
  PasswordForm form_1 =
      CreateEntry("first@google.com", "first", GURL("https://google.com/"),
                  PasswordForm::MatchType::kExact);
  PasswordForm form_2 =
      CreateEntry("second@google.com", "first", GURL("https://microsoft.com/"),
                  PasswordForm::MatchType::kExact);
  PasswordForm form_3 =
      CreateEntry("third@google.com", "second", GURL("https://netflix.com/"),
                  PasswordForm::MatchType::kExact);
  PasswordForm form_4 =
      CreateEntry("fourth@google.com", "second", GURL("https://amazon.com/"),
                  PasswordForm::MatchType::kExact);

  std::vector<Suggestion> suggestions =
      GenerateAllPasswordsSection({CredentialUIEntry({std::move(form_1)}),
                                   CredentialUIEntry({std::move(form_2)}),
                                   CredentialUIEntry({std::move(form_3)}),
                                   CredentialUIEntry({std::move(form_4)})},
                                  IsTriggeredOnPasswordForm(true));

  // Manual fallback suggestions are sorted by domain name.
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"amazon.com",
                              u"fourth@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://amazon.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"fourth@google.com", u"second",
                                  "https://amazon.com/", u"amazon.com",
                                  /*is_cross_domain=*/true)),
                          EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"google.com",
                              u"first@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://google.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"first@google.com", u"first",
                                  "https://google.com/", u"google.com",
                                  /*is_cross_domain=*/true)),
                          EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"microsoft.com",
                              u"second@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://microsoft.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"second@google.com", u"first",
                                  "https://microsoft.com/", u"microsoft.com",
                                  /*is_cross_domain=*/true)),
                          EqualsManualFallbackSuggestion(
                              SuggestionType::kPasswordEntry, u"netflix.com",
                              u"third@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::FaviconDetails(
                                  /*domain_url=*/GURL("https://netflix.com")),
                              Suggestion::PasswordSuggestionDetails(
                                  u"third@google.com", u"second",
                                  "https://netflix.com/", u"netflix.com",
                                  /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_ChildSuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form()}, IsTriggeredOnPasswordForm(true));

  // 1 password suggestion and 2 footer suggestions.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kPasswordFieldByFieldFilling,
                           u"username@example.com"),
          EqualsSuggestion(
              SuggestionType::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(
                  u"username@example.com", u"password", "https://google.com/",
                  u"google.com",
                  /*is_cross_domain=*/false)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kViewPasswordDetails,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
              Suggestion::Icon::kKey)));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_ChildSuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry()}, IsTriggeredOnPasswordForm(true));

  // 1 password suggestion and 2 footer suggestions.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kPasswordFieldByFieldFilling,
                           u"username@example.com"),
          EqualsSuggestion(
              SuggestionType::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(
                  u"username@example.com", u"password", "https://google.com/",
                  u"google.com",
                  /*is_cross_domain=*/true)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kViewPasswordDetails,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
              Suggestion::Icon::kKey)));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_NoUsername_ChildSuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form_no_username()}, IsTriggeredOnPasswordForm(true));

  // 1 password suggestion and 2 footer suggestions.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(
              SuggestionType::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(
                  u"", u"password", "https://google.com/", u"google.com",
                  /*is_cross_domain=*/false)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kViewPasswordDetails,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
              Suggestion::Icon::kKey)));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_NoUsername_ChildSuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry_no_username()}, IsTriggeredOnPasswordForm(true));

  // 1 password suggestion and 2 footer suggestions.
  EXPECT_EQ(suggestions.size(), 3u);
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(
              SuggestionType::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(
                  u"", u"password", "https://google.com/", u"google.com",
                  /*is_cross_domain=*/true)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kViewPasswordDetails,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
              Suggestion::Icon::kKey)));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_BothSections_OnlyAllPasswordsSectionIsSorted) {
  PasswordForm form_1 =
      CreateEntry("first@google.com", "first", GURL("https://microsoft.com/"),
                  PasswordForm::MatchType::kExact);

  PasswordForm form_2 =
      CreateEntry("second@google.com", "second", GURL("https://google.com/"),
                  PasswordForm::MatchType::kExact);

  PasswordForm form_3 =
      CreateEntry("third@google.com", "third", GURL("https://amazon.com/"),
                  PasswordForm::MatchType::kExact);

  std::vector<Suggestion> suggestions = GenerateBothSections(
      {form_1, form_2},
      {CredentialUIEntry({form_1}), CredentialUIEntry({form_3})},
      IsTriggeredOnPasswordForm(true));

  // Expected suggestions are:
  // 0. Suggested passwords section title.
  // 1. Suggestion for microsoft.com.
  // 2. Suggestion for google.com.
  // 3. All passwords section title.
  // 4. Suggestion for microsoft.com.
  // 5. Suggestion for amazon.com.
  // 6. Footer section separator.
  // 7. "Manage passwords" suggestion.
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(
              SuggestionType::kTitle,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_SUGGESTED_PASSWORDS_SECTION_TITLE)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"microsoft.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://microsoft.com/")),
              Suggestion::PasswordSuggestionDetails(
                  u"first@google.com", u"first", "https://microsoft.com/",
                  u"microsoft.com",
                  /*is_cross_domain=*/false)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              u"second@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://google.com/")),
              Suggestion::PasswordSuggestionDetails(
                  u"second@google.com", u"second", "https://google.com/",
                  u"google.com",
                  /*is_cross_domain=*/false)),
          EqualsSuggestion(
              SuggestionType::kTitle,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_ALL_PASSWORDS_SECTION_TITLE)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"amazon.com",
              u"third@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://amazon.com/")),
              Suggestion::PasswordSuggestionDetails(
                  u"third@google.com", u"third", "https://amazon.com/",
                  u"amazon.com",
                  /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"microsoft.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::FaviconDetails(
                  /*domain_url=*/GURL("https://microsoft.com/")),
              Suggestion::PasswordSuggestionDetails(
                  u"first@google.com", u"first", "https://microsoft.com/",
                  u"microsoft.com",
                  /*is_cross_domain=*/false)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_Favicons_CanBeRequestedFromGoogleIfSyncIsOn) {
  // Enable passwords syncing with no passphrase.
  EnablePasswordSync();

  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {CredentialUIEntry({password_form()})}, IsTriggeredOnPasswordForm(true));

  ASSERT_GE(suggestions.size(), 1u);
  EXPECT_THAT(suggestions[0], FaviconCanBeRequestedFromGoogle());
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_Favicons_CannotBeRequestedFromGoogleIfSyncIsOff) {
  // Passwords are not synced (`UserSelectableType::kPasswords` is not among
  // the selected types).
  EnablePasswordSync();
  EXPECT_CALL(*sync_service().GetMockUserSettings(), GetSelectedTypes)
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet({})));

  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {CredentialUIEntry({password_form()})}, IsTriggeredOnPasswordForm(true));

  ASSERT_GT(suggestions.size(), 1u);
  EXPECT_THAT(suggestions[0], Not(FaviconCanBeRequestedFromGoogle()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_Favicons_CannotBeRequestedFromGoogleIfPassphraseIsUsed) {
  // Enable passwords syncing with passphrase encryption.
  EnablePasswordSync();
  EXPECT_CALL(*sync_service().GetMockUserSettings(), IsUsingExplicitPassphrase)
      .WillRepeatedly(Return(true));

  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {CredentialUIEntry({password_form()})}, IsTriggeredOnPasswordForm(true));

  ASSERT_GT(suggestions.size(), 1u);
  EXPECT_THAT(suggestions[0], Not(FaviconCanBeRequestedFromGoogle()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_Favicons_CanBeRequestedFromGoogleForAccountPassword) {
  CredentialUIEntry credendials_1(
      {CreateEntry("example1@google.com", "first", GURL("https://amazon.com/"),
                   PasswordForm::MatchType::kExact)});
  CredentialUIEntry credendials_2(
      {CreateEntry("example2@google.com", "second", GURL("https://google.com/"),
                   PasswordForm::MatchType::kExact)});

  // Make the google.com password coming from user account.
  credendials_2.stored_in.insert(PasswordForm::Store::kAccountStore);

  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credendials_1, credendials_2}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(SuggestionHasFaviconDetails(Suggestion::FaviconDetails(
                      /*domain_url=*/GURL("https://amazon.com"),
                      /*can_be_requested_from_google=*/false)),
                  SuggestionHasFaviconDetails(Suggestion::FaviconDetails(
                      /*domain_url=*/GURL("https://google.com"),
                      /*can_be_requested_from_google=*/true)),
                  _, _));
}

TEST_F(
    PasswordSuggestionGeneratorTest,
    ManualFallback_Favicons_CannotBeRequestedFromGoogleForAccountPasswordIfPassphraseIsUsed) {
  EXPECT_CALL(*sync_service().GetMockUserSettings(), IsUsingExplicitPassphrase)
      .WillRepeatedly(Return(true));

  CredentialUIEntry credendials(
      {CreateEntry("example2@google.com", "second", GURL("https://google.com/"),
                   PasswordForm::MatchType::kExact)});
  credendials.stored_in.insert(PasswordForm::Store::kAccountStore);

  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credendials}, IsTriggeredOnPasswordForm(true));

  ASSERT_GT(suggestions.size(), 1u);
  EXPECT_THAT(suggestions[0], Not(FaviconCanBeRequestedFromGoogle()));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
