// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include <set>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;
using autofill::Suggestion;
using autofill::SuggestionType;

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

#if !BUILDFLAG(IS_ANDROID)
Suggestion CreatePasskeyFromAnotherDeviceEntry(bool listed_passkeys) {
  return Suggestion(
      l10n_util::GetStringUTF8(listed_passkeys
                                   ? IDS_PASSWORD_MANAGER_USE_DIFFERENT_PASSKEY
                                   : IDS_PASSWORD_MANAGER_USE_PASSKEY),
      /*label=*/"", Suggestion::Icon::kDevice,
      SuggestionType::kWebauthnSignInWithAnotherDevice);
}
#endif  // !BUILDFLAG(IS_ANDROID)

Suggestion CreateGenerationEntry() {
  // The UI code will pick up an icon from the resources based on the string.
  return Suggestion(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD),
      /*label=*/"", Suggestion::Icon::kKey,
      SuggestionType::kGeneratePasswordEntry);
}

// Entry for opting in to password account storage and then filling.
Suggestion CreateEntryToOptInToAccountStorageThenFill() {
  bool has_passkey_sync = false;
#if !BUILDFLAG(IS_ANDROID)
  has_passkey_sync =
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials);
#endif
  return Suggestion(
      l10n_util::GetStringUTF8(
          has_passkey_sync
              ? IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE_WITH_PASSKEYS
              : IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE),
      /*label=*/"", Suggestion::Icon::kGoogle,
      SuggestionType::kPasswordAccountStorageOptIn);
}

// Entry for opting in to password account storage and then generating password.
Suggestion CreateEntryToOptInToAccountStorageThenGenerate() {
  return Suggestion(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD),
      /*label=*/"", Suggestion::Icon::kKey,
      SuggestionType::kPasswordAccountStorageOptInAndGenerate);
}

// Entry for sigining in again which unlocks the password account storage.
Suggestion CreateEntryToReSignin() {
  return Suggestion(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_RE_SIGNIN_ACCOUNT_STORE),
      /*label=*/"", Suggestion::Icon::kGoogle,
      SuggestionType::kPasswordAccountStorageReSignin);
}

void MaybeAppendManagePasswordsEntry(std::vector<Suggestion>* suggestions) {
  bool has_no_fillable_suggestions = base::ranges::none_of(
      *suggestions,
      [](SuggestionType id) {
        return id == SuggestionType::kPasswordEntry ||
               id == SuggestionType::kAccountStoragePasswordEntry ||
               id == SuggestionType::kGeneratePasswordEntry ||
               id == SuggestionType::kWebauthnCredential;
      },
      &Suggestion::type);
  if (has_no_fillable_suggestions) {
    return;
  }

  bool has_webauthn_credential = base::ranges::any_of(
      *suggestions,
      [](SuggestionType type) {
        return type == SuggestionType::kWebauthnCredential;
      },
      &Suggestion::type);

  // Add a separator before the manage option unless there are no suggestions
  // yet.
  if (!suggestions->empty()) {
    Suggestion separator(SuggestionType::kSeparator);
    separator.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
    suggestions->push_back(std::move(separator));
  }

  Suggestion suggestion(
      l10n_util::GetStringUTF8(
          has_webauthn_credential
              ? IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS
              : IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
      /*label=*/"", Suggestion::Icon::kSettings,
      SuggestionType::kAllSavedPasswordsEntry);
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestions->emplace_back(std::move(suggestion));
}

// If |field_suggestion| matches |field_content|, creates a Suggestion out of it
// and appends to |suggestions|.
void AppendSuggestionIfMatching(const std::u16string& field_suggestion,
                                const std::u16string& field_contents,
                                const gfx::Image& custom_icon,
                                const std::string& signon_realm,
                                bool from_account_store,
                                size_t password_length,
                                std::vector<Suggestion>* suggestions) {
  std::u16string lower_suggestion = base::i18n::ToLower(field_suggestion);
  std::u16string lower_contents = base::i18n::ToLower(field_contents);
  if (base::StartsWith(lower_suggestion, lower_contents,
                       base::CompareCase::SENSITIVE)) {
    bool replaced_username;
    Suggestion suggestion(
        ReplaceEmptyUsername(field_suggestion, &replaced_username));
    suggestion.main_text.is_primary =
        Suggestion::Text::IsPrimary(!replaced_username);
    suggestion.labels = {{autofill::Suggestion::Text(
        std::u16string(password_length, constants::kPasswordReplacementChar))}};
    suggestion.voice_over = l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_PASSWORD_FOR_ACCOUNT, suggestion.main_text.value);
    if (!signon_realm.empty()) {
      // The domainname is only shown for passwords with a common eTLD+1
      // but different subdomain.
      suggestion.additional_label = GetHumanReadableRealm(signon_realm);
      *suggestion.voice_over += u", ";
      *suggestion.voice_over += suggestion.additional_label;
    }
    suggestion.type = from_account_store
                          ? SuggestionType::kAccountStoragePasswordEntry
                          : SuggestionType::kPasswordEntry;
    suggestion.custom_icon = custom_icon;
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = Suggestion::Icon::kGlobe;
    suggestions->emplace_back(std::move(suggestion));
  }
}

// This function attempts to fill |suggestions| from |fill_data| based on
// |current_username| that is the current value of the field.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const std::u16string& current_username,
                    const gfx::Image& custom_icon,
                    std::vector<Suggestion>* suggestions) {
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
            [](const Suggestion& a, const Suggestion& b) {
              return a.main_text.value < b.main_text.value;
            });
}

Suggestion CreateFillPasswordChildSuggestion(
    const CredentialUIEntry& credential,
    IsCrossDomain is_cross_origin) {
  Suggestion fill_password(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
      SuggestionType::kFillPassword);
  fill_password.payload = Suggestion::PasswordSuggestionDetails(
      credential.username, credential.password,
      credential.GetFirstSignonRealm(),
      GetHumanReadableRealm(credential.GetFirstSignonRealm()),
      is_cross_origin.value());
  return fill_password;
}

Suggestion CreateViewPasswordDetailsChildSuggestion(
    const Suggestion::PasswordSuggestionDetails& payload) {
  Suggestion view_password_details(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
      SuggestionType::kViewPasswordDetails);
  view_password_details.icon = Suggestion::Icon::kKey;
  view_password_details.payload = payload;
  return view_password_details;
}

void AppendManualFallbackSuggestions(
    const CredentialUIEntry& credential,
    IsTriggeredOnPasswordForm on_password_form,
    IsCrossDomain is_cross_origin,
    bool favicon_can_be_requested_from_google,
    std::vector<Suggestion>* suggestions,
    Suggestion::FiltrationPolicy filtration_policy) {
  // A separate suggestion with the same (username, password) pair is displayed
  // for every affiliated domain. For example, if the credential was saved on
  // apple.com and icloud.com, there will be 2 suggestions for both of these
  // websites.
  for (const CredentialUIEntry::DomainInfo& domain_info :
       credential.GetAffiliatedDomains()) {
    Suggestion suggestion(domain_info.name, /*label=*/"",
                          Suggestion::Icon::kGlobe,
                          SuggestionType::kPasswordEntry);
    bool replaced;
    const std::u16string maybe_username =
        ReplaceEmptyUsername(credential.username, &replaced);
    suggestion.labels = {{autofill::Suggestion::Text(maybe_username)}};
    Suggestion::PasswordSuggestionDetails payload(
        credential.username, credential.password, domain_info.signon_realm,
        /*display_signon_realm=*/base::UTF8ToUTF16(domain_info.name),
        is_cross_origin.value());
    suggestion.payload = payload;
    suggestion.is_acceptable = on_password_form.value();
    if (FacetURI::FromPotentiallyInvalidSpec(domain_info.signon_realm)
            .IsValidWebFacetURI()) {
      suggestion.custom_icon = Suggestion::FaviconDetails(
          domain_info.url, favicon_can_be_requested_from_google);
    }
    suggestion.filtration_policy = filtration_policy;

    if (!replaced) {
      suggestion.children.emplace_back(
          maybe_username, SuggestionType::kPasswordFieldByFieldFilling);
    }
    suggestion.children.push_back(
        CreateFillPasswordChildSuggestion(credential, is_cross_origin));
    suggestion.children.emplace_back(SuggestionType::kSeparator);
    suggestion.children.push_back(
        CreateViewPasswordDetailsChildSuggestion(payload));

    suggestions->emplace_back(std::move(suggestion));
  }
}

}  // namespace

PasswordSuggestionGenerator::PasswordSuggestionGenerator(
    PasswordManagerDriver* password_manager_driver,
    PasswordManagerClient* password_client)
    : password_manager_driver_(password_manager_driver),
      password_client_{password_client} {}

std::vector<Suggestion> PasswordSuggestionGenerator::GetSuggestionsForDomain(
    base::optional_ref<const autofill::PasswordFormFillData> fill_data,
    const gfx::Image& page_favicon,
    const std::u16string& username_filter,
    OffersGeneration offers_generation,
    ShowPasswordSuggestions show_password_suggestions,
    ShowWebAuthnCredentials show_webauthn_credentials) const {
  std::vector<Suggestion> suggestions;
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
          Suggestion suggestion(
              base::UTF16ToUTF8(ToUsernameString(passkey.username())),
              /*label=*/"", Suggestion::Icon::kGlobe,
              SuggestionType::kWebauthnCredential);
          suggestion.custom_icon = page_favicon;
          suggestion.payload =
              Suggestion::Guid(base::Base64Encode(passkey.credential_id()));
          suggestion.labels = {
              {Suggestion::Text(passkey.GetAuthenticatorLabel())}};
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
  // Add "Use a passkey" or "Use a different passkey" button.
  if (uses_passkeys && delegate->IsSecurityKeyOrHybridFlowAvailable()) {
#if !BUILDFLAG(IS_IOS)
    const bool passkey_from_another_device_in_autofill =
        !(base::FeatureList::IsEnabled(
              features::kPasswordManualFallbackAvailable) &&
          base::FeatureList::IsEnabled(
              features::kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu));
#else
    const bool passkey_from_another_device_in_autofill = true;
#endif  //! BUILDFLAG(IS_IOS)
    if (passkey_from_another_device_in_autofill) {
      bool listed_passkeys = delegate->GetPasskeys().has_value() &&
                             delegate->GetPasskeys()->size() > 0;
      suggestions.emplace_back(
          CreatePasskeyFromAnotherDeviceEntry(listed_passkeys));
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Add password generation entry, if available.
  if (offers_generation) {
    suggestions.emplace_back(
        show_account_storage_optin
            ? CreateEntryToOptInToAccountStorageThenGenerate()
            : CreateGenerationEntry());
  }

  // Add button to opt into using the account storage for passwords and then
  // suggest.
  if (show_account_storage_optin) {
    suggestions.emplace_back(CreateEntryToOptInToAccountStorageThenFill());
  }

  // Add button to sign-in which unlocks the previously used account store.
  if (show_account_storage_resignin) {
    suggestions.emplace_back(CreateEntryToReSignin());
  }

  // Add "Manage all passwords" link to settings.
  MaybeAppendManagePasswordsEntry(&suggestions);

  return suggestions;
}

std::vector<Suggestion>
PasswordSuggestionGenerator::GetManualFallbackSuggestions(
    base::span<const PasswordForm> suggested_credentials,
    base::span<const CredentialUIEntry> credentials,
    IsTriggeredOnPasswordForm on_password_form) const {
  std::vector<Suggestion> suggestions;
  const bool generate_sections =
      !suggested_credentials.empty() && !credentials.empty();
  if (generate_sections) {
    Suggestion title(
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_SUGGESTED_PASSWORDS_SECTION_TITLE),
        SuggestionType::kTitle);
    title.filtration_policy =
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter;
    suggestions.push_back(std::move(title));
  }

  auto* sync_service = password_client_->GetSyncService();
  const bool is_sync_passwords_enabled =
      sync_service &&
      password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          sync_service);
  const bool is_passphrase_user =
      sync_service &&
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase();
  std::set<std::string> suggested_signon_realms;
  for (const auto& form : suggested_credentials) {
    suggested_signon_realms.insert(form.signon_realm);
    const CredentialUIEntry ui_entry = CredentialUIEntry(form);
    const bool is_from_account =
        ui_entry.stored_in.contains(PasswordForm::Store::kAccountStore);
    const bool favicon_can_be_requested_from_google =
        (is_sync_passwords_enabled || is_from_account) && !is_passphrase_user;
    AppendManualFallbackSuggestions(
        ui_entry, on_password_form, IsCrossDomain(false),
        favicon_can_be_requested_from_google, &suggestions,
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter);
  }

  if (generate_sections) {
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_ALL_PASSWORDS_SECTION_TITLE),
        SuggestionType::kTitle);
    suggestions.back().filtration_policy =
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter;
  }

  // Only the "All passwords" section should be sorted alphabetically.
  const size_t relevant_section_offset = suggestions.size();

  for (const CredentialUIEntry& credential : credentials) {
    // Check if any credential in the "Suggested" section has the same singon
    // realm as this `CredentialUIEntry`.
    const bool has_suggested_realm = base::ranges::any_of(
        credential.facets,
        [&suggested_signon_realms](const std::string& signon_realm) {
          return suggested_signon_realms.count(signon_realm);
        },
        &CredentialFacet::signon_realm);
    const bool is_from_account =
        credential.stored_in.contains(PasswordForm::Store::kAccountStore);
    const bool favicon_can_be_requested_from_google =
        (is_sync_passwords_enabled || is_from_account) && !is_passphrase_user;
    AppendManualFallbackSuggestions(
        credential, on_password_form, IsCrossDomain(!has_suggested_realm),
        favicon_can_be_requested_from_google, &suggestions,
        Suggestion::FiltrationPolicy::kFilterable);
  }

  base::ranges::sort(
      suggestions.begin() + relevant_section_offset, suggestions.end(),
      base::ranges::less(),
      [](const Suggestion& suggestion) { return suggestion.main_text.value; });

  // Add "Manage all passwords" link to settings.
  MaybeAppendManagePasswordsEntry(&suggestions);

  return suggestions;
}

}  // namespace password_manager
