// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <stddef.h>

#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/favicon/core/favicon_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::password_generation::PasswordGenerationType;
using IsLoading = autofill::Suggestion::IsLoading;

constexpr char16_t kPasswordReplacementChar = 0x2022;

// Returns |username| unless it is empty. For an empty |username| returns a
// localised string saying this username is empty. Use this for displaying the
// usernames to the user. |replaced| is set to true iff |username| is empty.
std::u16string ReplaceEmptyUsername(const std::u16string& username,
                                    bool* replaced) {
  *replaced = username.empty();
  if (username.empty())
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  return username;
}

// Returns the prettified version of |signon_realm| to be displayed on the UI.
std::u16string GetHumanReadableRealm(const std::string& signon_realm) {
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
std::u16string GetUsernameFromSuggestion(const std::u16string& suggestion) {
  return suggestion ==
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? std::u16string()
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
    const std::u16string& field_suggestion,
    const std::u16string& field_contents,
    const gfx::Image& custom_icon,
    const std::string& signon_realm,
    bool show_all,
    bool is_password_field,
    bool from_account_store,
    size_t password_length,
    std::vector<autofill::Suggestion>* suggestions) {
  std::u16string lower_suggestion = base::i18n::ToLower(field_suggestion);
  std::u16string lower_contents = base::i18n::ToLower(field_contents);
  if (show_all || autofill::FieldIsSuggestionSubstringStartingOnTokenBoundary(
                      lower_suggestion, lower_contents, true)) {
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
    if (from_account_store) {
      suggestion.popup_item_id =
          is_password_field
              ? autofill::PopupItemId::kAccountStoragePasswordEntry
              : autofill::PopupItemId::kAccountStorageUsernameEntry;
    } else {
      suggestion.popup_item_id = is_password_field
                                     ? autofill::PopupItemId::kPasswordEntry
                                     : autofill::PopupItemId::kUsernameEntry;
    }
    suggestion.match =
        show_all || base::StartsWith(lower_suggestion, lower_contents,
                                     base::CompareCase::SENSITIVE)
            ? autofill::Suggestion::PREFIX_MATCH
            : autofill::Suggestion::SUBSTRING_MATCH;
    suggestion.custom_icon = custom_icon;
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = "globeIcon";
    suggestion.trailing_icon = CreateStoreIcon(from_account_store);
    suggestions->push_back(suggestion);
  }
}

// This function attempts to fill |suggestions| from |fill_data| based on
// |current_username| that is the current value of the field. Unless |show_all|
// is true, it only picks suggestions allowed by
// FieldIsSuggestionSubstringStartingOnTokenBoundary. It can pick either a
// substring or a prefix based on the flag.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const std::u16string& current_username,
                    const gfx::Image& custom_icon,
                    bool show_all,
                    bool is_password_field,
                    std::vector<autofill::Suggestion>* suggestions) {
  AppendSuggestionIfMatching(
      fill_data.preferred_login.username_value, current_username, custom_icon,
      fill_data.preferred_login.realm, show_all, is_password_field,
      fill_data.preferred_login.uses_account_store,
      fill_data.preferred_login.password_value.size(), suggestions);

  int prefered_match = suggestions->size();

  for (const auto& login : fill_data.additional_logins) {
    AppendSuggestionIfMatching(login.username_value, current_username,
                               custom_icon, login.realm, show_all,
                               is_password_field, login.uses_account_store,
                               login.password_value.size(), suggestions);
  }

  std::sort(suggestions->begin() + prefered_match, suggestions->end(),
            [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
              return a.main_text.value < b.main_text.value;
            });

  // Prefix matches should precede other token matches.
  if (!show_all && autofill::IsFeatureSubstringMatchEnabled()) {
    // Using stable sort in order to preserve sorting by 'value'
    std::stable_sort(
        suggestions->begin(), suggestions->end(),
        [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
          return a.match < b.match;
        });
  }
}

void MaybeAppendManagePasswordsEntry(
    std::vector<autofill::Suggestion>* suggestions) {
  bool has_no_fillable_suggestions = base::ranges::none_of(
      *suggestions,
      [](autofill::PopupItemId id) {
        return id == autofill::PopupItemId::kUsernameEntry ||
               id == autofill::PopupItemId::kPasswordEntry ||
               id == autofill::PopupItemId::kAccountStorageUsernameEntry ||
               id == autofill::PopupItemId::kAccountStoragePasswordEntry ||
               id == autofill::PopupItemId::kGeneratePasswordEntry ||
               id == autofill::PopupItemId::kWebauthnCredential;
      },
      &autofill::Suggestion::popup_item_id);
  if (has_no_fillable_suggestions)
    return;

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
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    // The UI code will pick up an icon from the resources based on the string.
    suggestion.icon = "settingsIcon";
  }
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.trailing_icon = "googlePasswordManager";
  suggestions->push_back(std::move(suggestion));
}

#if !BUILDFLAG(IS_ANDROID)
autofill::Suggestion CreateWebAuthnEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DEVICE_PASSKEY));
  suggestion.icon = "device";
  suggestion.popup_item_id =
      autofill::PopupItemId::kWebauthnSignInWithAnotherDevice;
  return suggestion;
}
#endif  // !BUILDFLAG(IS_ANDROID)

autofill::Suggestion CreateGenerationEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  // The UI code will pick up an icon from the resources based on the string.
  suggestion.icon = "keyIcon";
  suggestion.popup_item_id = autofill::PopupItemId::kGeneratePasswordEntry;
  return suggestion;
}

// Entry for opting in to password account storage and then filling.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenFill() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_OPT_INTO_ACCOUNT_STORE));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageOptIn;
  suggestion.icon = "google";
  return suggestion;
}

// Entry for opting in to password account storage and then generating password.
autofill::Suggestion CreateEntryToOptInToAccountStorageThenGenerate() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate;
  suggestion.icon = "keyIcon";
  return suggestion;
}

// Entry for sigining in again which unlocks the password account storage.
autofill::Suggestion CreateEntryToReSignin() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_RE_SIGNIN_ACCOUNT_STORE));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageReSignin;
  suggestion.icon = "google";
  return suggestion;
}

// Entry showing the empty state (i.e. no passwords found in account-storage).
autofill::Suggestion CreateAccountStorageEmptyEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_NO_ACCOUNT_STORE_MATCHES));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageEmpty;
  suggestion.icon = "empty";
  return suggestion;
}

bool ContainsOtherThanManagePasswords(
    base::span<const autofill::Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& s) {
    return s.popup_item_id != autofill::PopupItemId::kAllSavedPasswordsEntry;
  });
}

bool AreSuggestionForPasswordField(
    base::span<const autofill::Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& suggestion) {
    return suggestion.popup_item_id == autofill::PopupItemId::kPasswordEntry;
  });
}

bool HasLoadingSuggestion(base::span<const autofill::Suggestion> suggestions,
                          autofill::PopupItemId item_id) {
  return base::ranges::any_of(suggestions, [&item_id](const auto& suggestion) {
    return suggestion.popup_item_id == item_id && suggestion.is_loading;
  });
}

std::vector<autofill::Suggestion> SetUnlockLoadingState(
    base::span<const autofill::Suggestion> suggestions,
    autofill::PopupItemId unlock_item,
    IsLoading is_loading) {
  DCHECK(unlock_item == autofill::PopupItemId::kPasswordAccountStorageOptIn ||
         unlock_item ==
             autofill::PopupItemId::kPasswordAccountStorageReSignin ||
         unlock_item ==
             autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate);
  std::vector<autofill::Suggestion> new_suggestions;
  new_suggestions.reserve(suggestions.size());
  base::ranges::copy(suggestions, std::back_inserter(new_suggestions));
  auto unlock_iter = base::ranges::find(new_suggestions, unlock_item,
                                        &autofill::Suggestion::popup_item_id);
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
  if (fill_data.preferred_login.uses_account_store) {
    ++account_store_passwords_count;
  }
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
  CancelBiometricReauthIfOngoing();

  if (deletion_callback_)
    std::move(deletion_callback_).Run();
}

void PasswordAutofillManager::OnPopupShown() {}

void PasswordAutofillManager::OnPopupHidden() {}

void PasswordAutofillManager::OnPopupSuppressed() {}

void PasswordAutofillManager::DidSelectSuggestion(
    const autofill::Suggestion& suggestion) {
  ClearPreviewedForm();
  if (suggestion.popup_item_id ==
          autofill::PopupItemId::kAllSavedPasswordsEntry ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kPasswordAccountStorageEmpty ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kGeneratePasswordEntry ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kPasswordAccountStorageOptIn ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kPasswordAccountStorageReSignin ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate ||
      suggestion.popup_item_id ==
          autofill::PopupItemId::kWebauthnSignInWithAnotherDevice) {
    return;
  }

  PreviewSuggestion(GetUsernameFromSuggestion(suggestion.main_text.value),
                    suggestion.popup_item_id);
}

void PasswordAutofillManager::OnUnlockItemAccepted(
    autofill::PopupItemId unlock_item) {
  using metrics_util::PasswordDropdownSelectedOption;
  DCHECK(unlock_item == autofill::PopupItemId::kPasswordAccountStorageOptIn ||
         unlock_item ==
             autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate);

  UpdatePopup(SetUnlockLoadingState(autofill_client_->GetPopupSuggestions(),
                                    unlock_item, IsLoading(true)));
  signin_metrics::ReauthAccessPoint reauth_access_point =
      unlock_item == autofill::PopupItemId::kPasswordAccountStorageOptIn
          ? signin_metrics::ReauthAccessPoint::kAutofillDropdown
          : signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown;
  password_client_->TriggerReauthForPrimaryAccount(
      reauth_access_point,
      base::BindOnce(&PasswordAutofillManager::OnUnlockReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), unlock_item,
                     autofill_client_->GetReopenPopupArgs()));
}

void PasswordAutofillManager::DidAcceptSuggestion(
    const autofill::Suggestion& suggestion,
    int position) {
  using metrics_util::PasswordDropdownSelectedOption;
  switch (suggestion.popup_item_id) {
    case autofill::PopupItemId::kGeneratePasswordEntry:
      password_client_->GeneratePassword(PasswordGenerationType::kAutomatic);
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kGenerate,
          password_client_->IsOffTheRecord());
      break;
    case autofill::PopupItemId::kAllSavedPasswordsEntry:
    case autofill::PopupItemId::kPasswordAccountStorageEmpty:
      password_client_->NavigateToManagePasswordsPage(
          ManagePasswordsReferrer::kPasswordDropdown);
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kShowAll,
          password_client_->IsOffTheRecord());

      if (password_client_->GetMetricsRecorder()) {
        using UserAction = password_manager::PasswordManagerMetricsRecorder::
            PageLevelUserAction;
        password_client_->GetMetricsRecorder()->RecordPageLevelUserAction(
            UserAction::kShowAllPasswordsWhileSomeAreSuggested);
      }
      break;
    case autofill::PopupItemId::kPasswordAccountStorageReSignin:
      password_client_->TriggerSignIn(
          signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kResigninToUnlockAccountStore,
          password_client_->IsOffTheRecord());
      break;
    case autofill::PopupItemId::kPasswordAccountStorageOptIn:
    case autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate:
      OnUnlockItemAccepted(suggestion.popup_item_id);
      metrics_util::LogPasswordDropdownItemSelected(
          suggestion.popup_item_id ==
                  autofill::PopupItemId::kPasswordAccountStorageOptIn
              ? PasswordDropdownSelectedOption::kUnlockAccountStorePasswords
              : PasswordDropdownSelectedOption::kUnlockAccountStoreGeneration,
          password_client_->IsOffTheRecord());
      break;
    case autofill::PopupItemId::kWebauthnCredential:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kWebAuthn,
          password_client_->IsOffTheRecord());
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->SelectPasskey(
              absl::holds_alternative<autofill::Suggestion::BackendId>(
                  suggestion.payload)
                  ? absl::get<autofill::Suggestion::BackendId>(
                        suggestion.payload)
                        .value()
                  : std::string());
      break;
    case autofill::PopupItemId::kWebauthnSignInWithAnotherDevice:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kWebAuthnSignInWithAnotherDevice,
          password_client_->IsOffTheRecord());
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->LaunchWebAuthnFlow();
      break;
    default:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kPassword,
          password_client_->IsOffTheRecord());

      CancelBiometricReauthIfOngoing();
      scoped_refptr<device_reauth::DeviceAuthenticator> authenticator =
          password_client_->GetDeviceAuthenticator();
      // Note: this is currently only implemented on Android, Mac and Windows.
      // For other platforms, the `authenticator` will be null.
      if (!password_manager_util::CanUseBiometricAuth(authenticator.get(),
                                                      password_client_)) {
        bool success = FillSuggestion(
            GetUsernameFromSuggestion(suggestion.main_text.value),
            suggestion.popup_item_id);
        DCHECK(success);
      } else {
        authenticator_ = std::move(authenticator);
#if BUILDFLAG(IS_ANDROID)
        authenticator_->Authenticate(
            device_reauth::DeviceAuthRequester::kAutofillSuggestion,
            base::BindOnce(&PasswordAutofillManager::OnBiometricReauthCompleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           suggestion.main_text.value,
                           suggestion.popup_item_id),
            /*use_last_valid_auth=*/true);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        const std::u16string origin =
            base::UTF8ToUTF16(GetShownOrigin(url::Origin::Create(
                password_manager_driver_->GetLastCommittedURL())));

        auto on_reath_complete = base::BindOnce(
            &PasswordAutofillManager::OnBiometricReauthCompleted,
            weak_ptr_factory_.GetWeakPtr(), suggestion.main_text.value,
            suggestion.popup_item_id);

        authenticator_->AuthenticateWithMessage(
            l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH,
                                       origin),
            metrics_util::TimeCallback(
                std::move(on_reath_complete),
                "PasswordManager.PasswordFilling.AuthenticationTime"));
#endif
      }
      break;
  }

  autofill_client_->HideAutofillPopup(
      autofill::PopupHidingReason::kAcceptSuggestion);
}

bool PasswordAutofillManager::GetDeletionConfirmationText(
    const std::u16string& value,
    autofill::PopupItemId popup_item_id,
    autofill::Suggestion::BackendId backend_id,
    std::u16string* title,
    std::u16string* body) {
  return false;
}

bool PasswordAutofillManager::RemoveSuggestion(
    const std::u16string& value,
    autofill::PopupItemId popup_item_id,
    autofill::Suggestion::BackendId backend_id) {
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

absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordAutofillManager::GetDriver() {
  return password_manager_driver_.get();
}

int32_t PasswordAutofillManager::GetWebContentsPopupControllerAxId() const {
  // TODO: Needs to be implemented when we step up accessibility features in the
  // future.
  // See http://crbug.com/991253
  NOTIMPLEMENTED_LOG_ONCE();
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

  // If the `fill_data_` changes, then it's likely that the filling context
  // changed as well, so the biometric auth is now out of scope.
  CancelBiometricReauthIfOngoing();
  RequestFavicon(fill_data.url);

  // If there are no username or password suggestions, WebAuthn credentials
  // can still cause a popup to appear.
  if (fill_data.preferred_login.username_value.empty() &&
      fill_data.preferred_login.password_value.empty()) {
    return;
  }

  fill_data_ = std::make_unique<autofill::PasswordFormFillData>(fill_data);

  if (!autofill_client_ || autofill_client_->GetPopupSuggestions().empty())
    return;
  // Only log account-stored passwords if the unlock just happened.
  if (HasLoadingSuggestion(
          autofill_client_->GetPopupSuggestions(),
          autofill::PopupItemId::kPasswordAccountStorageOptIn)) {
    LogAccountStoredPasswordsCountInFillDataAfterUnlock(fill_data);
  }
  UpdatePopup(BuildSuggestions(std::u16string(),
                               ForPasswordField(AreSuggestionForPasswordField(
                                   autofill_client_->GetPopupSuggestions())),
                               ShowAllPasswords(true), OffersGeneration(false),
                               ShowPasswordSuggestions(true),
                               ShowWebAuthnCredentials(false)));
}

void PasswordAutofillManager::OnNoCredentialsFound() {
  if (!autofill_client_ ||
      !HasLoadingSuggestion(
          autofill_client_->GetPopupSuggestions(),
          autofill::PopupItemId::kPasswordAccountStorageOptIn)) {
    return;
  }
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
  CancelBiometricReauthIfOngoing();
}

void PasswordAutofillManager::OnShowPasswordSuggestions(
    base::i18n::TextDirection text_direction,
    const std::u16string& typed_username,
    int options,
    const gfx::RectF& bounds) {
  ShowPopup(
      bounds, text_direction,
      BuildSuggestions(typed_username,
                       ForPasswordField(options & autofill::IS_PASSWORD_FIELD),
                       ShowAllPasswords(options & autofill::SHOW_ALL),
                       OffersGeneration(false), ShowPasswordSuggestions(true),
                       ShowWebAuthnCredentials(
                           options & autofill::ACCEPTS_WEBAUTHN_CREDENTIALS)));
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestions(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  return ShowPopup(
      bounds, text_direction,
      BuildSuggestions(std::u16string(), ForPasswordField(true),
                       ShowAllPasswords(true), OffersGeneration(false),
                       ShowPasswordSuggestions(true),
                       ShowWebAuthnCredentials(false)));
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestionsWithGeneration(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    bool show_password_suggestions) {
  return ShowPopup(
      bounds, text_direction,
      BuildSuggestions(std::u16string(), ForPasswordField(true),
                       ShowAllPasswords(true), OffersGeneration(true),
                       ShowPasswordSuggestions(show_password_suggestions),
                       ShowWebAuthnCredentials(false)));
}

void PasswordAutofillManager::DidNavigateMainFrame() {
  fill_data_.reset();
  CancelBiometricReauthIfOngoing();
  favicon_tracker_.TryCancelAll();
  page_favicon_ = gfx::Image();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    const std::u16string& username) {
  return FillSuggestion(username, autofill::PopupItemId::kPasswordEntry);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    const std::u16string& username) {
  return PreviewSuggestion(username, autofill::PopupItemId::kPasswordEntry);
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

std::vector<autofill::Suggestion> PasswordAutofillManager::BuildSuggestions(
    const std::u16string& username_filter,
    ForPasswordField for_password_field,
    ShowAllPasswords show_all_passwords,
    OffersGeneration offers_generation,
    ShowPasswordSuggestions show_password_suggestions,
    ShowWebAuthnCredentials show_webauthn_credentials) {
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
        [this](const auto& passkey) {
          autofill::Suggestion suggestion(ToUsernameString(passkey.username()));
          suggestion.icon = "globeIcon";
          suggestion.popup_item_id = autofill::PopupItemId::kWebauthnCredential;
          suggestion.custom_icon = page_favicon_;
          suggestion.payload = autofill::Suggestion::BackendId(
              base::Base64Encode(passkey.credential_id()));
          suggestion.labels = {{autofill::Suggestion::Text(
              l10n_util::GetStringUTF16(passkey.GetAuthenticatorLabel()))}};
          return suggestion;
        });
  }

  if (!fill_data_ && !show_account_storage_optin &&
      !show_account_storage_resignin && !uses_passkeys && suggestions.empty()) {
    // Probably the credential was deleted in the mean time.
    return suggestions;
  }

  // Add password suggestions if they exist and were requested.
  if (show_password_suggestions && fill_data_) {
    GetSuggestions(*fill_data_, username_filter, page_favicon_,
                   show_all_passwords.value(), for_password_field.value(),
                   &suggestions);
  }

#if !BUILDFLAG(IS_ANDROID)
  // Add "Sign in with another device" button.
  if (uses_passkeys) {
    suggestions.push_back(CreateWebAuthnEntry());
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
  if (show_account_storage_optin)
    suggestions.push_back(CreateEntryToOptInToAccountStorageThenFill());

  // Add button to sign-in which unlocks the previously used account store.
  if (show_account_storage_resignin)
    suggestions.push_back(CreateEntryToReSignin());

  // Add "Manage all passwords" link to settings.
  MaybeAppendManagePasswordsEntry(&suggestions);

  return suggestions;
}

void PasswordAutofillManager::LogMetricsForSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) const {
  metrics_util::PasswordDropdownState dropdown_state =
      metrics_util::PasswordDropdownState::kStandard;
  for (const auto& suggestion : suggestions) {
    switch (suggestion.popup_item_id) {
      case autofill::PopupItemId::kGeneratePasswordEntry:
        // TODO(crbug.com/1062709): Revisit metrics for the "opt in and
        // generate" button.
      case autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate:
        dropdown_state = metrics_util::PasswordDropdownState::kStandardGenerate;
        break;
      default:
        break;
    }
  }
  metrics_util::LogPasswordDropdownShown(dropdown_state,
                                         password_client_->IsOffTheRecord());
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
      bounds, text_direction, suggestions,
      autofill::AutoselectFirstSuggestion(false));
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

bool PasswordAutofillManager::FillSuggestion(
    const std::u16string& username,
    autofill::PopupItemId popup_item_id) {
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ &&
      GetPasswordAndMetadataForUsername(username, popup_item_id, *fill_data_,
                                        &password_and_meta_data)) {
    bool is_android_credential =
        FacetURI::FromPotentiallyInvalidSpec(password_and_meta_data.realm)
            .IsValidAndroidFacetURI();
    metrics_util::LogFilledPasswordFromAndroidApp(is_android_credential);
    password_manager_driver_->FillSuggestion(
        username, password_and_meta_data.password_value);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::PreviewSuggestion(
    const std::u16string& username,
    autofill::PopupItemId popup_item_id) {
  if (popup_item_id == autofill::PopupItemId::kWebauthnCredential) {
    password_manager_driver_->PreviewSuggestion(username, /*password=*/u"");
    return true;
  }
  if (password_client_->GetPasswordFeatureManager()
          ->IsBiometricAuthenticationBeforeFillingEnabled()) {
    return false;
  }
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ &&
      GetPasswordAndMetadataForUsername(username, popup_item_id, *fill_data_,
                                        &password_and_meta_data)) {
    password_manager_driver_->PreviewSuggestion(
        username, password_and_meta_data.password_value);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::GetPasswordAndMetadataForUsername(
    const std::u16string& current_username,
    autofill::PopupItemId popup_item_id,
    const autofill::PasswordFormFillData& fill_data,
    autofill::PasswordAndMetadata* password_and_meta_data) {
  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use |password_manager_client_| here to
  // fetch the actual password. See crbug.com/178358 for more context.

  bool item_uses_account_store =
      popup_item_id == autofill::PopupItemId::kAccountStorageUsernameEntry ||
      popup_item_id == autofill::PopupItemId::kAccountStoragePasswordEntry;

  // Look for any suitable matches to current field text.
  if (fill_data.preferred_login.username_value == current_username &&
      fill_data.preferred_login.uses_account_store == item_uses_account_store) {
    password_and_meta_data->username_value = current_username;
    password_and_meta_data->password_value =
        fill_data.preferred_login.password_value;
    password_and_meta_data->realm = fill_data.preferred_login.realm;
    password_and_meta_data->uses_account_store =
        fill_data.preferred_login.uses_account_store;
    return true;
  }

  // Scan additional logins for a match.
  auto iter = base::ranges::find_if(
      fill_data.additional_logins,
      [&](const autofill::PasswordAndMetadata& login) {
        return current_username == login.username_value &&
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
        autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate) {
      password_client_->GeneratePassword(PasswordGenerationType::kAutomatic);
      autofill_client_->HideAutofillPopup(
          autofill::PopupHidingReason::kAcceptSuggestion);
    }
    return;
  }
  UpdatePopup(SetUnlockLoadingState(reopen_args.suggestions, unlock_item,
                                    IsLoading(false)));
}

void PasswordAutofillManager::OnBiometricReauthCompleted(
    const std::u16string& value,
    autofill::PopupItemId popup_item_id,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded)
    return;
  bool success =
      FillSuggestion(GetUsernameFromSuggestion(value), popup_item_id);
  DCHECK(success);
}

void PasswordAutofillManager::CancelBiometricReauthIfOngoing() {
  if (!authenticator_)
    return;
  authenticator_->Cancel(
      device_reauth::DeviceAuthRequester::kAutofillSuggestion);
  authenticator_.reset();
}

}  //  namespace password_manager
