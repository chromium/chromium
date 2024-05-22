// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include <vector>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
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
using testing::AllOf;
using testing::Conditional;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::NiceMock;
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
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(id, main_text, icon),
      Field(
          "labels", &Suggestion::labels,
          ElementsAre(ElementsAre(autofill::Suggestion::Text(username_label)))),
      Field("is_acceptable", &Suggestion::is_acceptable, is_acceptable),
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

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

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
    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&credentials_delegate()));
  }

  const gfx::Image& favicon() const { return favicon_; }

  StubPasswordManagerDriver& driver() { return driver_; }

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

 private:
  gfx::Image favicon_;

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
// Verify that suggestion for account store credential receives a different
// `SuggestionType` and trailing icon.
// TODO crbug/40943570: Remove after feature is fully rolled out.
TEST_F(PasswordSuggestionGeneratorTest,
       PasswordSuggestions_FromAccountStore_ButterFollowupDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kButterOnDesktopFollowup);
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
                              Suggestion::Icon::kGoogle),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

// Verify that the trailing icon is not set for the account store credential if
// the `kButterOnDesktopFollowup` is enabled.
TEST_F(PasswordSuggestionGeneratorTest, PasswordSuggestions_FromAccountStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kButterOnDesktopFollowup);
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
      {{syncer::kSyncWebauthnCredentials, false},
       {password_manager::features::kButterOnDesktopFollowup, true}});

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
                              Suggestion::PasswordSuggestionDetails(
                                  u"password", u"google.com",
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
                              Suggestion::PasswordSuggestionDetails(
                                  u"password", u"google.com",
                                  /*is_cross_domain=*/true)),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_AndroidCredential_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {android_credential_ui_entry()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsManualFallbackSuggestion(
                      SuggestionType::kPasswordEntry, u"Netflix",
                      u"username@example.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/true,
                      Suggestion::PasswordSuggestionDetails(
                          u"password", u"Netflix", /*is_cross_domain=*/true)),
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
                              Suggestion::PasswordSuggestionDetails(
                                  u"password", u"google.com",
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
                              Suggestion::PasswordSuggestionDetails(
                                  u"password", u"google.com",
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
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"amazon.com",
              u"example@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"password", u"amazon.com",
                                                    /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              u"example@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"amazon.com",
              u"fourth@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"second", u"amazon.com",
                                                    /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"first", u"google.com",
                                                    /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"microsoft.com",
              u"second@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"first", u"microsoft.com",
                                                    /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"netflix.com",
              u"third@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"second", u"netflix.com",
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
              Suggestion::PasswordSuggestionDetails(u"password", u"google.com",
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
              Suggestion::PasswordSuggestionDetails(u"first", u"microsoft.com",
                                                    /*is_cross_domain=*/false)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"google.com",
              u"second@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"second", u"google.com",
                                                    /*is_cross_domain=*/false)),
          EqualsSuggestion(
              SuggestionType::kTitle,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_ALL_PASSWORDS_SECTION_TITLE)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"amazon.com",
              u"third@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"third", u"amazon.com",
                                                    /*is_cross_domain=*/true)),
          EqualsManualFallbackSuggestion(
              SuggestionType::kPasswordEntry, u"microsoft.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"first", u"microsoft.com",
                                                    /*is_cross_domain=*/false)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePasswordsSuggestion()));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
