// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include "base/base64.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/sync/base/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;

constexpr char16_t kPasswordReplacementChar = 0x2022;

// Returns |username| unless it is empty. For an empty |username| returns a
// localised string saying this username is empty. Use this for displaying the
// usernames to the user. |replaced| is set to true iff |username| is empty.
std::u16string ReplaceEmptyUsername(const std::u16string& username,
                                    bool* replaced) {
  *replaced = username.empty();
  if (username.empty()) {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  }
  return username;
}

// Returns the prettified version of |signon_realm| to be displayed on the UI.
std::u16string GetHumanReadableRealm(const std::string& signon_realm) {
  // For Android application realms, remove the hash component. Otherwise, make
  // no changes.
  FacetURI maybe_facet_uri(FacetURI::FromPotentiallyInvalidSpec(signon_realm));
  if (maybe_facet_uri.IsValidAndroidFacetURI()) {
    return base::UTF8ToUTF16("android://" +
                             maybe_facet_uri.android_package_name() + "/");
  }
  GURL realm(signon_realm);
  if (realm.is_valid()) {
    return base::UTF8ToUTF16(realm.host());
  }
  return base::UTF8ToUTF16(signon_realm);
}

// Returns a string representing the icon of either the account store or the
// local password store.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
autofill::Suggestion::Icon CreateStoreIcon(bool for_account_store) {
  return for_account_store ? autofill::Suggestion::Icon::kGoogle
                           : autofill::Suggestion::Icon::kNoIcon;
}
#endif

#if !BUILDFLAG(IS_ANDROID)
autofill::Suggestion CreateWebAuthnEntry(bool listed_passkeys) {
  autofill::Suggestion suggestion(l10n_util::GetStringUTF16(
      listed_passkeys ? IDS_PASSWORD_MANAGER_USE_DIFFERENT_PASSKEY
                      : IDS_PASSWORD_MANAGER_USE_PASSKEY));
  suggestion.icon = autofill::Suggestion::Icon::kDevice;
  suggestion.popup_item_id =
      autofill::PopupItemId::kWebauthnSignInWithAnotherDevice;
  return suggestion;
}
#endif  // !BUILDFLAG(IS_ANDROID)

autofill::Suggestion CreateGenerationEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.icon = autofill::Suggestion::Icon::kKey;
  suggestion.popup_item_id = autofill::PopupItemId::kGeneratePasswordEntry;
  return suggestion;
}

// Entry for opting in to password account storage and then filling.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenFill() {
  bool has_passkey_sync = false;
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  has_passkey_sync =
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials);
#endif
  autofill::Suggestion suggestion(l10n_util::GetStringUTF16(
      has_passkey_sync
          ? IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE_WITH_PASSKEYS
          : IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageOptIn;
  suggestion.icon = autofill::Suggestion::Icon::kGoogle;
  return suggestion;
}

// Entry for opting in to password account storage and then generating password.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenGenerate() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate;
  suggestion.icon = autofill::Suggestion::Icon::kKey;
  return suggestion;
}

// Entry for sigining in again which unlocks the password account storage.
autofill::Suggestion CreateEntryToReSignin() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_RE_SIGNIN_ACCOUNT_STORE));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageReSignin;
  suggestion.icon = autofill::Suggestion::Icon::kGoogle;
  return suggestion;
}

void MaybeAppendManagePasswordsEntry(
    std::vector<autofill::Suggestion>* suggestions) {
  bool has_no_fillable_suggestions = base::ranges::none_of(
      *suggestions,
      [](autofill::PopupItemId id) {
        return id == autofill::PopupItemId::kPasswordEntry ||
               id == autofill::PopupItemId::kAccountStoragePasswordEntry ||
               id == autofill::PopupItemId::kGeneratePasswordEntry ||
               id == autofill::PopupItemId::kWebauthnCredential;
      },
      &autofill::Suggestion::popup_item_id);
  if (has_no_fillable_suggestions) {
    return;
  }

  bool has_webauthn_credential = base::ranges::any_of(
      *suggestions,
      [](autofill::PopupItemId popup_item_id) {
        return popup_item_id == autofill::PopupItemId::kWebauthnCredential;
      },
      &autofill::Suggestion::popup_item_id);

#if !BUILDFLAG(IS_ANDROID)
  // Add a separator before the manage option unless there are no suggestions
  // yet.
  // TODO(crbug.com/1274134): Clean up once improvements are launched.
  if (!suggestions->empty()) {
    suggestions->emplace_back(autofill::PopupItemId::kSeparator);
  }
#endif

  autofill::Suggestion suggestion(l10n_util::GetStringUTF16(
      has_webauthn_credential
          ? IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS
          : IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  suggestion.popup_item_id = autofill::PopupItemId::kAllSavedPasswordsEntry;
  suggestion.icon = autofill::Suggestion::Icon::kSettings;
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.trailing_icon = autofill::Suggestion::Icon::kGooglePasswordManager;
  suggestions->push_back(std::move(suggestion));
}

// If |field_suggestion| matches |field_content|, creates a Suggestion out of it
// and appends to |suggestions|.
void AppendSuggestionIfMatching(
    const std::u16string& field_suggestion,
    const std::u16string& field_contents,
    const gfx::Image& custom_icon,
    const std::string& signon_realm,
    bool from_account_store,
    size_t password_length,
    std::vector<autofill::Suggestion>* suggestions) {
  std::u16string lower_suggestion = base::i18n::ToLower(field_suggestion);
  std::u16string lower_contents = base::i18n::ToLower(field_contents);
  if (base::StartsWith(lower_suggestion, lower_contents,
                       base::CompareCase::SENSITIVE)) {
    bool replaced_username;
    autofill::Suggestion suggestion(
        ReplaceEmptyUsername(field_suggestion, &replaced_username));
    suggestion.main_text.is_primary =
        autofill::Suggestion::Text::IsPrimary(!replaced_username);
    suggestion.labels = {
        {autofill::Suggestion::Text(GetHumanReadableRealm(signon_realm))}};
    suggestion.additional_label =
        std::u16string(password_length, kPasswordReplacementChar);
    suggestion.voice_over = l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_PASSWORD_FOR_ACCOUNT, suggestion.main_text.value);
    if (!suggestion.labels.empty()) {
      // The domainname is only shown for passwords with a common eTLD+1
      // but different subdomain.
      DCHECK_EQ(suggestion.labels.size(), 1U);
      DCHECK_EQ(suggestion.labels[0].size(), 1U);
      *suggestion.voice_over += u", ";
      *suggestion.voice_over += suggestion.labels[0][0].value;
    }
    suggestion.popup_item_id =
        from_account_store ? autofill::PopupItemId::kAccountStoragePasswordEntry
                           : autofill::PopupItemId::kPasswordEntry;
    suggestion.custom_icon = custom_icon;
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = autofill::Suggestion::Icon::kGlobe;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    if (!base::FeatureList::IsEnabled(
            password_manager::features::kButterOnDesktopFollowup)) {
      suggestion.trailing_icon = CreateStoreIcon(from_account_store);
    }
#endif
    suggestions->push_back(suggestion);
  }
}

// This function attempts to fill |suggestions| from |fill_data| based on
// |current_username| that is the current value of the field.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const std::u16string& current_username,
                    const gfx::Image& custom_icon,
                    std::vector<autofill::Suggestion>* suggestions) {
  AppendSuggestionIfMatching(
      fill_data.preferred_login.username_value, current_username, custom_icon,
      fill_data.preferred_login.realm,
      fill_data.preferred_login.uses_account_store,
      fill_data.preferred_login.password_value.size(), suggestions);

  int prefered_match = suggestions->size();

  for (const auto& login : fill_data.additional_logins) {
    AppendSuggestionIfMatching(
        login.username_value, current_username, custom_icon, login.realm,
        login.uses_account_store, login.password_value.size(), suggestions);
  }

  std::sort(suggestions->begin() + prefered_match, suggestions->end(),
            [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
              return a.main_text.value < b.main_text.value;
            });
}

void AddPasswordUsernameChildSuggestion(const std::u16string& username,
                                        autofill::Suggestion& suggestion) {
  suggestion.children.push_back(autofill::Suggestion(
      username, autofill::PopupItemId::kPasswordFieldByFieldFilling));
}

void AddFillPasswordChildSuggestion(autofill::Suggestion& suggestion) {
  suggestion.children.push_back(autofill::Suggestion(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
      autofill::PopupItemId::kFillPassword));
}

void AddViewPasswordDetailsChildSuggestion(autofill::Suggestion& suggestion) {
  autofill::Suggestion view_password_details(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
      autofill::PopupItemId::kViewPasswordDetails);
  view_password_details.icon = autofill::Suggestion::Icon::kKey;
  suggestion.children.push_back(view_password_details);
}

autofill::Suggestion GetManualFallbackSuggestion(
    const CredentialUIEntry& credential) {
  autofill::Suggestion suggestion(
      GetHumanReadableRealm(credential.GetFirstSignonRealm()),
      autofill::PopupItemId::kPasswordEntry);
  bool replaced;
  const std::u16string maybe_username =
      ReplaceEmptyUsername(credential.username, &replaced);
  suggestion.additional_label = maybe_username;
  suggestion.icon = autofill::Suggestion::Icon::kGlobe;

  if (!replaced) {
    AddPasswordUsernameChildSuggestion(maybe_username, suggestion);
  }
  AddFillPasswordChildSuggestion(suggestion);
  suggestion.children.push_back(
      autofill::Suggestion(autofill::PopupItemId::kSeparator));
  AddViewPasswordDetailsChildSuggestion(suggestion);

  return suggestion;
}

}  // namespace

PasswordSuggestionGenerator::PasswordSuggestionGenerator(
    PasswordManagerDriver* password_manager_driver,
    PasswordManagerClient* password_client)
    : password_manager_driver_(password_manager_driver),
      password_client_{password_client} {}

std::vector<autofill::Suggestion>
PasswordSuggestionGenerator::GetSuggestionsForDomain(
    base::optional_ref<const autofill::PasswordFormFillData> fill_data,
    const gfx::Image& page_favicon,
    const std::u16string& username_filter,
    OffersGeneration offers_generation,
    ShowPasswordSuggestions show_password_suggestions,
    ShowWebAuthnCredentials show_webauthn_credentials) const {
  std::vector<autofill::Suggestion> suggestions;
  bool show_account_storage_optin =
      password_client_ && password_client_->GetPasswordFeatureManager()
                              ->ShouldShowAccountStorageOptIn();
  bool show_account_storage_resignin =
      password_client_ && password_client_->GetPasswordFeatureManager()
                              ->ShouldShowAccountStorageReSignin(
                                  password_client_->GetLastCommittedURL());

  // Add WebAuthn credentials suitable for an ongoing request if available.
  WebAuthnCredentialsDelegate* delegate =
      password_client_->GetWebAuthnCredentialsDelegateForDriver(
          password_manager_driver_);
  // |uses_passkeys| is used on desktop only to offer a way to sign in with a
  // passkey on another device. On Android this is always false. It also will
  // not be set on iOS since |show_webauthn_credentials| is always false.
  bool uses_passkeys = false;
  if (show_webauthn_credentials && delegate &&
      delegate->GetPasskeys().has_value()) {
#if !BUILDFLAG(IS_ANDROID)
    uses_passkeys = true;
#endif
    base::ranges::transform(
        *delegate->GetPasskeys(), std::back_inserter(suggestions),
        [&page_favicon](const auto& passkey) {
          autofill::Suggestion suggestion(ToUsernameString(passkey.username()));
          suggestion.icon = autofill::Suggestion::Icon::kGlobe;
          suggestion.popup_item_id = autofill::PopupItemId::kWebauthnCredential;
          suggestion.custom_icon = page_favicon;
          suggestion.payload = autofill::Suggestion::Guid(
              base::Base64Encode(passkey.credential_id()));
          suggestion.labels = {
              {autofill::Suggestion::Text(passkey.GetAuthenticatorLabel())}};
          return suggestion;
        });
  }

  if (!fill_data.has_value() && !show_account_storage_optin &&
      !show_account_storage_resignin && !uses_passkeys && suggestions.empty()) {
    // Probably the credential was deleted in the mean time.
    return suggestions;
  }

  // Add password suggestions if they exist and were requested.
  if (show_password_suggestions && fill_data.has_value()) {
    GetSuggestions(*fill_data, username_filter, page_favicon, &suggestions);
  }

#if !BUILDFLAG(IS_ANDROID)
  // Add "Sign in with another device" button.
  if (uses_passkeys && delegate->OfferPasskeysFromAnotherDeviceOption()) {
    bool listed_passkeys = delegate->GetPasskeys().has_value() &&
                           delegate->GetPasskeys()->size() > 0;
    suggestions.push_back(CreateWebAuthnEntry(listed_passkeys));
  }
#endif

  // Add password generation entry, if available.
  if (offers_generation) {
    suggestions.push_back(show_account_storage_optin
                              ? CreateEntryToOptInToAccountStorageThenGenerate()
                              : CreateGenerationEntry());
  }

  // Add button to opt into using the account storage for passwords and then
  // suggest.
  if (show_account_storage_optin) {
    suggestions.push_back(CreateEntryToOptInToAccountStorageThenFill());
  }

  // Add button to sign-in which unlocks the previously used account store.
  if (show_account_storage_resignin) {
    suggestions.push_back(CreateEntryToReSignin());
  }

  // Add "Manage all passwords" link to settings.
  MaybeAppendManagePasswordsEntry(&suggestions);

  return suggestions;
}

std::vector<autofill::Suggestion>
PasswordSuggestionGenerator::GetManualFallbackSuggestions(
    const std::vector<CredentialUIEntry>& credentials) const {
  std::vector<autofill::Suggestion> suggestions;
  for (const CredentialUIEntry& credential : credentials) {
    suggestions.push_back(GetManualFallbackSuggestion(credential));
  }

  base::ranges::sort(suggestions, [](const autofill::Suggestion& a,
                                     const autofill::Suggestion& b) {
    return a.main_text.value < b.main_text.value;
  });

  // Add "Manage all passwords" link to settings.
  MaybeAppendManagePasswordsEntry(&suggestions);

  return suggestions;
}

}  // namespace password_manager
