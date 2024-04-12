// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include <vector>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace password_manager {

using autofill::EqualsSuggestion;
using autofill::PasswordFormFillData;
using autofill::PopupItemId;
using autofill::Suggestion;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Matcher;

Matcher<Suggestion> EqualsManualFallbackSuggestion(
    PopupItemId id,
    const std::u16string& main_text,
    const std::u16string& additional_label,
    Suggestion::Icon icon,
    bool is_acceptable,
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(id, main_text, icon),
      Field("additional_label", &Suggestion::additional_label,
            additional_label),
      Field("is_acceptable", &Suggestion::is_acceptable, is_acceptable),
      Field("payload", &Suggestion::payload, payload));
}

Matcher<Suggestion> EqualsManageManagePasswordsSuggestion(
    bool has_webauthn_credential = false) {
  return AllOf(EqualsSuggestion(
                   PopupItemId::kAllSavedPasswordsEntry,
                   has_webauthn_credential
                       ? l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS)
                       : l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
                   Suggestion::Icon::kSettings),
               Field("trailing_icon", &Suggestion::trailing_icon,
                     Suggestion::Icon::kGooglePasswordManager));
}

// TODO(b/323316649): Write more tests for password suggestion generation.
class PasswordSuggestionGeneratorTest : public testing::Test {
 public:
  PasswordSuggestionGeneratorTest() : generator_(&driver(), &client()) {}

  const gfx::Image& favicon() const { return favicon_; }

  StubPasswordManagerDriver& driver() { return driver_; }

  StubPasswordManagerClient& client() { return client_; }

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

  StubPasswordManagerClient client_;
  StubPasswordManagerDriver driver_;
  PasswordSuggestionGenerator generator_;
};

// Test that no suggestions are generated from an empty `PasswordFormFillData`.
TEST_F(PasswordSuggestionGeneratorTest, NoPasswordFormFillData) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      {}, favicon(), /*username_filter=*/u"", OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));

  EXPECT_TRUE(suggestions.empty());
}

// Manual fallback suggestions are only relevant for desktop platform.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PasswordSuggestionGeneratorTest, ManualFallback_NoCredentials) {
  std::vector<Suggestion> suggestions = GenerateBothSections(
      std::vector<PasswordForm>(), std::vector<CredentialUIEntry>(),
      IsTriggeredOnPasswordForm(true));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsManualFallbackSuggestion(
                      PopupItemId::kPasswordEntry, u"google.com",
                      u"username@example.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/true,
                      Suggestion::PasswordSuggestionDetails(u"password")),
                  EqualsSuggestion(PopupItemId::kSeparator),
                  EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsManualFallbackSuggestion(
                      PopupItemId::kPasswordEntry, u"google.com",
                      u"username@example.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/true,
                      Suggestion::PasswordSuggestionDetails(u"password")),
                  EqualsSuggestion(PopupItemId::kSeparator),
                  EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_NoUsername_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form_no_username()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"google.com",
              l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN),
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_NoUsername_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry_no_username()}, IsTriggeredOnPasswordForm(true));

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"google.com",
              l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN),
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_SuggestedPasswords_NonPasswordForm_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateSuggestedPasswordsSection(
      {password_form()}, IsTriggeredOnPasswordForm(false));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsManualFallbackSuggestion(
                      PopupItemId::kPasswordEntry, u"google.com",
                      u"username@example.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/false,
                      Suggestion::PasswordSuggestionDetails(u"password")),
                  EqualsSuggestion(PopupItemId::kSeparator),
                  EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_NonPasswordForm_SuggestionContent) {
  std::vector<Suggestion> suggestions = GenerateAllPasswordsSection(
      {credential_ui_entry()}, IsTriggeredOnPasswordForm(false));

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsManualFallbackSuggestion(
                      PopupItemId::kPasswordEntry, u"google.com",
                      u"username@example.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/false,
                      Suggestion::PasswordSuggestionDetails(u"password")),
                  EqualsSuggestion(PopupItemId::kSeparator),
                  EqualsManageManagePasswordsSuggestion()));
}

TEST_F(PasswordSuggestionGeneratorTest,
       ManualFallback_AllPasswords_FirstDomainIsUsed) {
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
      ElementsAre(EqualsManualFallbackSuggestion(
                      PopupItemId::kPasswordEntry, u"google.com",
                      u"example@google.com", Suggestion::Icon::kGlobe,
                      /*is_acceptable=*/true,
                      Suggestion::PasswordSuggestionDetails(u"password")),
                  EqualsSuggestion(PopupItemId::kSeparator),
                  EqualsManageManagePasswordsSuggestion()));
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
                              PopupItemId::kPasswordEntry, u"amazon.com",
                              u"fourth@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::PasswordSuggestionDetails(u"second")),
                          EqualsManualFallbackSuggestion(
                              PopupItemId::kPasswordEntry, u"google.com",
                              u"first@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::PasswordSuggestionDetails(u"first")),
                          EqualsManualFallbackSuggestion(
                              PopupItemId::kPasswordEntry, u"microsoft.com",
                              u"second@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::PasswordSuggestionDetails(u"first")),
                          EqualsManualFallbackSuggestion(
                              PopupItemId::kPasswordEntry, u"netflix.com",
                              u"third@google.com", Suggestion::Icon::kGlobe,
                              /*is_acceptable=*/true,
                              Suggestion::PasswordSuggestionDetails(u"second")),
                          EqualsSuggestion(PopupItemId::kSeparator),
                          EqualsManageManagePasswordsSuggestion()));
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
          EqualsSuggestion(PopupItemId::kPasswordFieldByFieldFilling,
                           u"username@example.com"),
          EqualsSuggestion(
              PopupItemId::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(
              PopupItemId::kViewPasswordDetails,
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
          EqualsSuggestion(PopupItemId::kPasswordFieldByFieldFilling,
                           u"username@example.com"),
          EqualsSuggestion(
              PopupItemId::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(
              PopupItemId::kViewPasswordDetails,
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
              PopupItemId::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(
              PopupItemId::kViewPasswordDetails,
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
              PopupItemId::kFillPassword,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
              Suggestion::Icon::kNoIcon,
              Suggestion::PasswordSuggestionDetails(u"password")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(
              PopupItemId::kViewPasswordDetails,
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

  std::vector<Suggestion> suggestions = GenerateBothSections(
      {form_1, form_2},
      {CredentialUIEntry({form_1}), CredentialUIEntry({form_2})},
      IsTriggeredOnPasswordForm(true));

  // Expected suggestions are:
  // 0. Suggested passwords section title.
  // 1. Suggestion for microsoft.com.
  // 2. Suggestion for google.com.
  // 3. All passwords section title.
  // 4. Suggestion for google.com.
  // 5. Suggestion for microsoft.com.
  // 6. Footer section separator.
  // 7. "Manage passwords" suggestion.
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(
              PopupItemId::kTitle,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_SUGGESTED_PASSWORDS_SECTION_TITLE)),
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"microsoft.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"first")),
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"google.com", u"second@google.com",
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"second")),
          EqualsSuggestion(
              PopupItemId::kTitle,
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_ALL_PASSWORDS_SECTION_TITLE)),
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"google.com", u"second@google.com",
              Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"second")),
          EqualsManualFallbackSuggestion(
              PopupItemId::kPasswordEntry, u"microsoft.com",
              u"first@google.com", Suggestion::Icon::kGlobe,
              /*is_acceptable=*/true,
              Suggestion::PasswordSuggestionDetails(u"first")),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsManageManagePasswordsSuggestion()));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
