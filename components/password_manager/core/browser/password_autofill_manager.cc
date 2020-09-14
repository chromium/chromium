// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/favicon/core/favicon_util.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using AutoselectFirstSuggestion =
    autofill::AutofillClient::PopupOpenArgs::AutoselectFirstSuggestion;
using IsLoading = autofill::Suggestion::IsLoading;

constexpr base::char16 kPasswordReplacementChar = 0x2022;

// Returns |username| unless it is empty. For an empty |username| returns a
// localised string saying this username is empty. Use this for displaying the
// usernames to the user. |replaced| is set to true iff |username| is empty.
base::string16 ReplaceEmptyUsername(const base::string16& username,
                                    bool* replaced) {
  *replaced = username.empty();
  if (username.empty())
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  return username;
}

// Returns the prettified version of |signon_realm| to be displayed on the UI.
base::string16 GetHumanReadableRealm(const std::string& signon_realm) {
  // For Android application realms, remove the hash component. Otherwise, make
  // no changes.
  FacetURI maybe_facet_uri(FacetURI::FromPotentiallyInvalidSpec(signon_realm));
  if (maybe_facet_uri.IsValidAndroidFacetURI())
    return base::UTF8ToUTF16("android://" +
                             maybe_facet_uri.android_package_name() + "/");
  GURL realm(signon_realm);
  if (realm.is_valid())
    return base::UTF8ToUTF16(realm.host());
  return base::UTF8ToUTF16(signon_realm);
}

// If |suggestion| was made for an empty username, then return the empty
// string, otherwise return |suggestion|.
base::string16 GetUsernameFromSuggestion(const base::string16& suggestion) {
  return suggestion ==
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? base::string16()
             : suggestion;
}

// Returns a string representing the icon of either the account store or the
// local password store.
std::string CreateStoreIcon(bool for_account_store) {
  return for_account_store ? "google" : std::string();
}

// If |field_suggestion| matches |field_content|, creates a Suggestion out of it
// and appends to |suggestions|.
void AppendSuggestionIfMatching(
    const base::string16& field_suggestion,
    const base::string16& field_contents,
    const gfx::Image& custom_icon,
    const std::string& signon_realm,
    bool show_all,
    bool is_password_field,
    bool from_account_store,
    size_t password_length,
    std::vector<autofill::Suggestion>* suggestions) {
  base::string16 lower_suggestion = base::i18n::ToLower(field_suggestion);
  base::string16 lower_contents = base::i18n::ToLower(field_contents);
  if (show_all || autofill::FieldIsSuggestionSubstringStartingOnTokenBoundary(
                      lower_suggestion, lower_contents, true)) {
    bool replaced_username;
    autofill::Suggestion suggestion(
        ReplaceEmptyUsername(field_suggestion, &replaced_username));
    suggestion.is_value_secondary = replaced_username;
    suggestion.label = GetHumanReadableRealm(signon_realm);
    suggestion.additional_label =
        base::string16(password_length, kPasswordReplacementChar);
    if (from_account_store) {
      suggestion.frontend_id =
          is_password_field
              ? autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY
              : autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY;
    } else {
      suggestion.frontend_id = is_password_field
                                   ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                                   : autofill::POPUP_ITEM_ID_USERNAME_ENTRY;
    }
    suggestion.match =
        show_all || base::StartsWith(lower_suggestion, lower_contents,
                                     base::CompareCase::SENSITIVE)
            ? autofill::Suggestion::PREFIX_MATCH
            : autofill::Suggestion::SUBSTRING_MATCH;
    suggestion.custom_icon = custom_icon;
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = "globeIcon";
    suggestion.store_indicator_icon = CreateStoreIcon(from_account_store);
    suggestions->push_back(suggestion);
  }
}

// This function attempts to fill |suggestions| from |fill_data| based on
// |current_username| that is the current value of the field. Unless |show_all|
// is true, it only picks suggestions allowed by
// FieldIsSuggestionSubstringStartingOnTokenBoundary. It can pick either a
// substring or a prefix based on the flag.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const base::string16& current_username,
                    const gfx::Image& custom_icon,
                    bool show_all,
                    bool is_password_field,
                    std::vector<autofill::Suggestion>* suggestions) {
  AppendSuggestionIfMatching(fill_data.username_field.value, current_username,
                             custom_icon, fill_data.preferred_realm, show_all,
                             is_password_field, fill_data.uses_account_store,
                             fill_data.password_field.value.size(),
                             suggestions);

  for (const auto& login : fill_data.additional_logins) {
    AppendSuggestionIfMatching(login.username, current_username, custom_icon,
                               login.realm, show_all, is_password_field,
                               login.uses_account_store, login.password.size(),
                               suggestions);
  }

  // Prefix matches should precede other token matches.
  if (!show_all && autofill::IsFeatureSubstringMatchEnabled()) {
    std::sort(suggestions->begin(), suggestions->end(),
              [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
                return a.match < b.match;
              });
  }
}

void MaybeAppendManualFallback(std::vector<autofill::Suggestion>* suggestions) {
  bool has_no_fillable_suggestions = base::ranges::none_of(
      *suggestions,
      [](int id) {
        return id == autofill::POPUP_ITEM_ID_USERNAME_ENTRY ||
               id == autofill::POPUP_ITEM_ID_PASSWORD_ENTRY ||
               id == autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY ||
               id == autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY ||
               id == autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY;
      },
      &autofill::Suggestion::frontend_id);
  if (has_no_fillable_suggestions)
    return;
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  suggestion.frontend_id = autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = "settingsIcon";
  }
  suggestions->push_back(std::move(suggestion));
}

autofill::Suggestion CreateGenerationEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.icon = "keyIcon";
  suggestion.frontend_id = autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY;
  return suggestion;
}

// Entry for opting in to password account storage and then filling.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenFill() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE));
  suggestion.frontend_id =
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN;
  suggestion.icon = "google";
  return suggestion;
}

// Entry for opting in to password account storage and then generating password.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenGenerate() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  suggestion.frontend_id =
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE;
  suggestion.icon = "google";
  return suggestion;
}

// Entry for sigining in again which unlocks the password account storage.
autofill::Suggestion CreateEntryToReSignin() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_RE_SIGNIN_ACCOUNT_STORE));
  suggestion.frontend_id =
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN;
  suggestion.icon = "google";
  return suggestion;
}

// Entry showing the empty state (i.e. no passwords found in account-storage).
autofill::Suggestion CreateAccountStorageEmptyEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_NO_ACCOUNT_STORE_MATCHES));
  suggestion.frontend_id =
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY;
  suggestion.icon = "empty";
  return suggestion;
}

bool ContainsOtherThanManagePasswords(
    base::span<const autofill::Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& s) {
    return s.frontend_id != autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY;
  });
}

bool AreSuggestionForPasswordField(
    base::span<const autofill::Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& suggestion) {
    return suggestion.frontend_id == autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;
  });
}

bool HasLoadingSuggestion(base::span<const autofill::Suggestion> suggestions,
                          autofill::PopupItemId item_id) {
  return base::ranges::any_of(suggestions, [&item_id](const auto& suggestion) {
    return suggestion.frontend_id == item_id && suggestion.is_loading;
  });
}

std::vector<autofill::Suggestion> SetUnlockLoadingState(
    base::span<const autofill::Suggestion> suggestions,
    autofill::PopupItemId unlock_item,
    IsLoading is_loading) {
  DCHECK(
      unlock_item == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN ||
      unlock_item ==
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN ||
      unlock_item ==
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE);
  std::vector<autofill::Suggestion> new_suggestions;
  new_suggestions.reserve(suggestions.size());
  std::copy(suggestions.begin(), suggestions.end(),
            std::back_inserter(new_suggestions));
  auto unlock_iter =
      std::find_if(new_suggestions.begin(), new_suggestions.end(),
                   [unlock_item](const autofill::Suggestion& suggestion) {
                     return suggestion.frontend_id == unlock_item;
                   });
  unlock_iter->is_loading = is_loading;
  return new_suggestions;
}

void LogAccountStoredPasswordsCountInFillDataAfterUnlock(
    const autofill::PasswordFormFillData& fill_data) {
  int account_store_passwords_count =
      base::ranges::count_if(fill_data.additional_logins,
                             [](const autofill::PasswordAndMetadata& metadata) {
                               return metadata.uses_account_store;
                             });
  if (fill_data.uses_account_store)
    ++account_store_passwords_count;
  metrics_util::LogPasswordsCountFromAccountStoreAfterUnlock(
      account_store_passwords_count);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client,
    PasswordManagerClient* password_client)
    : password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      password_client_(password_client) {}

PasswordAutofillManager::~PasswordAutofillManager() {
  if (deletion_callback_)
    std::move(deletion_callback_).Run();
}

void PasswordAutofillManager::OnPopupShown() {}

void PasswordAutofillManager::OnPopupHidden() {}

void PasswordAutofillManager::OnPopupSuppressed() {}

void PasswordAutofillManager::DidSelectSuggestion(const base::string16& value,
                                                  int identifier) {
  ClearPreviewedForm();
  if (identifier == autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY ||
      identifier == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY ||
      identifier == autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY ||
      identifier == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN ||
      identifier ==
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN ||
      identifier ==
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE)
    return;
  bool success =
      PreviewSuggestion(GetUsernameFromSuggestion(value), identifier);
  DCHECK(success);
}

void PasswordAutofillManager::OnUnlockItemAccepted(
    autofill::PopupItemId unlock_item) {
  using metrics_util::PasswordDropdownSelectedOption;
  DCHECK(
      unlock_item == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN ||
      unlock_item ==
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE);

  UpdatePopup(SetUnlockLoadingState(autofill_client_->GetPopupSuggestions(),
                                    unlock_item, IsLoading(true)));
  signin_metrics::ReauthAccessPoint reauth_access_point =
      unlock_item == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN
          ? signin_metrics::ReauthAccessPoint::kAutofillDropdown
          : signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown;
  password_client_->TriggerReauthForPrimaryAccount(
      reauth_access_point,
      base::BindOnce(&PasswordAutofillManager::OnUnlockReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), unlock_item,
                     autofill_client_->GetReopenPopupArgs()));
}

void PasswordAutofillManager::DidAcceptSuggestion(const base::string16& value,
                                                  int identifier,
                                                  int position) {
  using metrics_util::PasswordDropdownSelectedOption;

  if (identifier == autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY) {
    password_client_->GeneratePassword();
    metrics_util::LogPasswordDropdownItemSelected(
        PasswordDropdownSelectedOption::kGenerate,
        password_client_->IsIncognito());
  } else if (identifier == autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY ||
             identifier ==
                 autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY) {
    password_client_->NavigateToManagePasswordsPage(
        ManagePasswordsReferrer::kPasswordDropdown);
    metrics_util::LogPasswordDropdownItemSelected(
        PasswordDropdownSelectedOption::kShowAll,
        password_client_->IsIncognito());

    if (password_client_->GetMetricsRecorder()) {
      using UserAction =
          password_manager::PasswordManagerMetricsRecorder::PageLevelUserAction;
      password_client_->GetMetricsRecorder()->RecordPageLevelUserAction(
          UserAction::kShowAllPasswordsWhileSomeAreSuggested);
    }
  } else if (identifier ==
             autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN) {
    password_client_->TriggerSignIn(
        signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
    metrics_util::LogPasswordDropdownItemSelected(
        PasswordDropdownSelectedOption::kResigninToUnlockAccountStore,
        password_client_->IsIncognito());
  } else if (
      identifier == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN ||
      identifier ==
          autofill::
              POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE) {
    OnUnlockItemAccepted(static_cast<autofill::PopupItemId>(identifier));
    metrics_util::LogPasswordDropdownItemSelected(
        identifier == autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN
            ? PasswordDropdownSelectedOption::kUnlockAccountStorePasswords
            : PasswordDropdownSelectedOption::kUnlockAccountStoreGeneration,
        password_client_->IsIncognito());
  } else {
    bool success = FillSuggestion(GetUsernameFromSuggestion(value), identifier);
    metrics_util::LogPasswordDropdownItemSelected(
        PasswordDropdownSelectedOption::kPassword,
        password_client_->IsIncognito());
    DCHECK(success);
  }

  autofill_client_->HideAutofillPopup(
      autofill::PopupHidingReason::kAcceptSuggestion);
}

bool PasswordAutofillManager::GetDeletionConfirmationText(
    const base::string16& value,
    int identifier,
    base::string16* title,
    base::string16* body) {
  return false;
}

bool PasswordAutofillManager::RemoveSuggestion(const base::string16& value,
                                               int identifier) {
  // Password suggestions cannot be deleted this way.
  // See http://crbug.com/329038#c15
  return false;
}

void PasswordAutofillManager::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
}

autofill::PopupType PasswordAutofillManager::GetPopupType() const {
  return autofill::PopupType::kPasswords;
}

autofill::AutofillDriver* PasswordAutofillManager::GetAutofillDriver() {
  return password_manager_driver_->GetAutofillDriver();
}

int32_t PasswordAutofillManager::GetWebContentsPopupControllerAxId() const {
  // TODO: Needs to be implemented when we step up accessibility features in the
  // future.
  NOTIMPLEMENTED_LOG_ONCE() << "See http://crbug.com/991253";
  return 0;
}

void PasswordAutofillManager::RegisterDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
}

void PasswordAutofillManager::OnAddPasswordFillData(
    const autofill::PasswordFormFillData& fill_data) {
  if (!autofill::IsValidPasswordFormFillData(fill_data))
    return;

  fill_data_ = std::make_unique<autofill::PasswordFormFillData>(fill_data);
  RequestFavicon(fill_data.url);

  if (!autofill_client_ || autofill_client_->GetPopupSuggestions().empty())
    return;
  // Only log account-stored passwords if the unlock just happened.
  if (HasLoadingSuggestion(
          autofill_client_->GetPopupSuggestions(),
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN)) {
    LogAccountStoredPasswordsCountInFillDataAfterUnlock(fill_data);
  }
  UpdatePopup(BuildSuggestions(base::string16(),
                               ForPasswordField(AreSuggestionForPasswordField(
                                   autofill_client_->GetPopupSuggestions())),
                               ShowAllPasswords(true), OffersGeneration(false),
                               ShowPasswordSuggestions(true)));
}

void PasswordAutofillManager::OnNoCredentialsFound() {
  if (!autofill_client_ ||
      !HasLoadingSuggestion(
          autofill_client_->GetPopupSuggestions(),
          autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN))
    return;
  metrics_util::LogPasswordsCountFromAccountStoreAfterUnlock(
      /*account_store_passwords_count=*/0);
  UpdatePopup({CreateAccountStorageEmptyEntry()});
}

void PasswordAutofillManager::DeleteFillData() {
  fill_data_.reset();
  if (autofill_client_) {
    autofill_client_->HideAutofillPopup(
        autofill::PopupHidingReason::kStaleData);
  }
}

void PasswordAutofillManager::OnShowPasswordSuggestions(
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    int options,
    const gfx::RectF& bounds) {
  ShowPopup(
      bounds, text_direction,
      BuildSuggestions(typed_username,
                       ForPasswordField(options & autofill::IS_PASSWORD_FIELD),
                       ShowAllPasswords(options & autofill::SHOW_ALL),
                       OffersGeneration(false), ShowPasswordSuggestions(true)));
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestions(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  return ShowPopup(
      bounds, text_direction,
      BuildSuggestions(base::string16(), ForPasswordField(true),
                       ShowAllPasswords(true), OffersGeneration(false),
                       ShowPasswordSuggestions(true)));
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestionsWithGeneration(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    bool show_password_suggestions) {
  return ShowPopup(
      bounds, text_direction,
      BuildSuggestions(base::string16(), ForPasswordField(true),
                       ShowAllPasswords(true), OffersGeneration(true),
                       ShowPasswordSuggestions(show_password_suggestions)));
}

void PasswordAutofillManager::DidNavigateMainFrame() {
  fill_data_.reset();
  favicon_tracker_.TryCancelAll();
  page_favicon_ = gfx::Image();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    const base::string16& username) {
  return FillSuggestion(username, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    const base::string16& username) {
  return PreviewSuggestion(username, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

std::vector<autofill::Suggestion> PasswordAutofillManager::BuildSuggestions(
    const base::string16& username_filter,
    ForPasswordField for_password_field,
    ShowAllPasswords show_all_passwords,
    OffersGeneration offers_generation,
    ShowPasswordSuggestions show_password_suggestions) {
  std::vector<autofill::Suggestion> suggestions;
  bool show_account_storage_optin =
      password_client_ && password_client_->GetPasswordFeatureManager()
                              ->ShouldShowAccountStorageOptIn();
  bool show_account_storage_resignin =
      password_client_ && password_client_->GetPasswordFeatureManager()
                              ->ShouldShowAccountStorageReSignin(
                                  password_client_->GetLastCommittedURL());

  if (!fill_data_ && !show_account_storage_optin &&
      !show_account_storage_resignin) {
    // Probably the credential was deleted in the mean time.
    return suggestions;
  }

  // Add password suggestions if they exist and were requested.
  if (show_password_suggestions && fill_data_) {
    GetSuggestions(*fill_data_, username_filter, page_favicon_,
                   show_all_passwords.value(), for_password_field.value(),
                   &suggestions);
  }

  // Add password generation entry, if available.
  if (offers_generation) {
    suggestions.push_back(show_account_storage_optin
                              ? CreateEntryToOptInToAccountStorageThenGenerate()
                              : CreateGenerationEntry());
  }

  // Add "Manage all passwords" link to settings.
  MaybeAppendManualFallback(&suggestions);

  // Add button to opt into using the account storage for passwords and then
  // suggest.
  if (show_account_storage_optin)
    suggestions.push_back(CreateEntryToOptInToAccountStorageThenFill());

  // Add button to sign-in which unlocks the previously used account store.
  if (show_account_storage_resignin)
    suggestions.push_back(CreateEntryToReSignin());

  return suggestions;
}

void PasswordAutofillManager::LogMetricsForSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) const {
  metrics_util::PasswordDropdownState dropdown_state =
      metrics_util::PasswordDropdownState::kStandard;
  for (const auto& suggestion : suggestions) {
    switch (suggestion.frontend_id) {
      case autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY:
        // TODO(crbug.com/1062709): Revisit metrics for the "opt in and
        // generate" button.
      case autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE:
        dropdown_state = metrics_util::PasswordDropdownState::kStandardGenerate;
        break;
    }
  }
  metrics_util::LogPasswordDropdownShown(dropdown_state,
                                         password_client_->IsIncognito());
}

bool PasswordAutofillManager::ShowPopup(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<autofill::Suggestion>& suggestions) {
  if (!password_manager_driver_->CanShowAutofillUi())
    return false;
  if (!ContainsOtherThanManagePasswords(suggestions)) {
    autofill_client_->HideAutofillPopup(
        autofill::PopupHidingReason::kNoSuggestions);
    return false;
  }
  LogMetricsForSuggestions(suggestions);
  autofill::AutofillClient::PopupOpenArgs open_args(
      bounds, text_direction, suggestions, AutoselectFirstSuggestion(false),
      autofill::PopupType::kPasswords);
  autofill_client_->ShowAutofillPopup(open_args,
                                      weak_ptr_factory_.GetWeakPtr());
  return true;
}

void PasswordAutofillManager::UpdatePopup(
    const std::vector<autofill::Suggestion>& suggestions) {
  if (!password_manager_driver_->CanShowAutofillUi())
    return;
  if (!ContainsOtherThanManagePasswords(suggestions)) {
    autofill_client_->HideAutofillPopup(
        autofill::PopupHidingReason::kNoSuggestions);
    return;
  }
  autofill_client_->UpdatePopup(suggestions, autofill::PopupType::kPasswords);
}

bool PasswordAutofillManager::FillSuggestion(const base::string16& username,
                                             int item_id) {
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ &&
      GetPasswordAndMetadataForUsername(username, item_id, *fill_data_,
                                        &password_and_meta_data)) {
    bool is_android_credential =
        FacetURI::FromPotentiallyInvalidSpec(password_and_meta_data.realm)
            .IsValidAndroidFacetURI();
    metrics_util::LogFilledCredentialIsFromAndroidApp(is_android_credential);
    password_manager_driver_->FillSuggestion(username,
                                             password_and_meta_data.password);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::PreviewSuggestion(const base::string16& username,
                                                int item_id) {
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ &&
      GetPasswordAndMetadataForUsername(username, item_id, *fill_data_,
                                        &password_and_meta_data)) {
    password_manager_driver_->PreviewSuggestion(
        username, password_and_meta_data.password);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::GetPasswordAndMetadataForUsername(
    const base::string16& current_username,
    int item_id,
    const autofill::PasswordFormFillData& fill_data,
    autofill::PasswordAndMetadata* password_and_meta_data) {
  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use |password_manager_client_| here to
  // fetch the actual password. See crbug.com/178358 for more context.

  bool item_uses_account_store =
      item_id == autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY ||
      item_id == autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY;

  // Look for any suitable matches to current field text.
  if (fill_data.username_field.value == current_username &&
      fill_data.uses_account_store == item_uses_account_store) {
    password_and_meta_data->username = current_username;
    password_and_meta_data->password = fill_data.password_field.value;
    password_and_meta_data->realm = fill_data.preferred_realm;
    password_and_meta_data->uses_account_store = fill_data.uses_account_store;
    return true;
  }

  // Scan additional logins for a match.
  auto iter = std::find_if(
      fill_data.additional_logins.begin(), fill_data.additional_logins.end(),
      [&current_username,
       &item_uses_account_store](const autofill::PasswordAndMetadata& login) {
        return current_username == login.username &&
               item_uses_account_store == login.uses_account_store;
      });
  if (iter != fill_data.additional_logins.end()) {
    *password_and_meta_data = *iter;
    return true;
  }

  return false;
}

void PasswordAutofillManager::RequestFavicon(const GURL& url) {
  if (!password_client_)
    return;
  favicon::GetFaviconImageForPageURL(
      password_client_->GetFaviconService(), url,
      favicon_base::IconType::kFavicon,
      base::BindOnce(&PasswordAutofillManager::OnFaviconReady,
                     weak_ptr_factory_.GetWeakPtr()),
      &favicon_tracker_);
}

void PasswordAutofillManager::OnFaviconReady(
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty())
    page_favicon_ = result.image;
}

void PasswordAutofillManager::OnUnlockReauthCompleted(
    autofill::PopupItemId unlock_item,
    autofill::AutofillClient::PopupOpenArgs reopen_args,
    PasswordManagerClient::ReauthSucceeded reauth_succeeded) {
  autofill_client_->ShowAutofillPopup(reopen_args,
                                      weak_ptr_factory_.GetWeakPtr());
  autofill_client_->PinPopupView();
  if (reauth_succeeded) {
    if (unlock_item ==
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE) {
      password_client_->GeneratePassword();
      autofill_client_->HideAutofillPopup(
          autofill::PopupHidingReason::kAcceptSuggestion);
    }
    return;
  }
  UpdatePopup(SetUnlockLoadingState(reopen_args.suggestions, unlock_item,
                                    IsLoading(false)));
}

}  //  namespace password_manager
