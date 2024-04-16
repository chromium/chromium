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

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/favicon/core/favicon_util.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_flow.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::password_generation::PasswordGenerationType;
using IsLoading = autofill::Suggestion::IsLoading;

// Entry showing the empty state (i.e. no passwords found in account-storage).
autofill::Suggestion CreateAccountStorageEmptyEntry() {
  autofill::Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_NO_ACCOUNT_STORE_MATCHES));
  suggestion.popup_item_id =
      autofill::PopupItemId::kPasswordAccountStorageEmpty;
  suggestion.icon = autofill::Suggestion::Icon::kEmpty;
  return suggestion;
}

// If `suggestion` was made for an empty username, then return the empty
// string, otherwise return `suggestion`.
std::u16string GetUsernameFromSuggestion(const std::u16string& suggestion) {
  return suggestion ==
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? std::u16string()
             : suggestion;
}

bool ContainsOtherThanManagePasswords(
    base::span<const autofill::Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& s) {
    return s.popup_item_id != autofill::PopupItemId::kAllSavedPasswordsEntry;
  });
}

bool HasLoadingSuggestion(base::span<const autofill::Suggestion> suggestions,
                          autofill::PopupItemId item_id) {
  return base::ranges::any_of(suggestions, [&item_id](const auto& suggestion) {
    return suggestion.popup_item_id == item_id && suggestion.is_loading;
  });
}

std::string GetBackendId(const autofill::Suggestion& suggestion) {
  return absl::holds_alternative<autofill::Suggestion::BackendId>(
             suggestion.payload)
             ? suggestion.GetBackendId<autofill::Suggestion::Guid>().value()
             : std::string();
}

std::vector<autofill::Suggestion> SetUnlockLoadingState(
    std::vector<autofill::Suggestion> suggestions,
    autofill::PopupItemId unlock_item,
    IsLoading is_loading) {
  DCHECK(unlock_item == autofill::PopupItemId::kPasswordAccountStorageOptIn ||
         unlock_item ==
             autofill::PopupItemId::kPasswordAccountStorageReSignin ||
         unlock_item ==
             autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate);
  auto unlock_iter = base::ranges::find(suggestions, unlock_item,
                                        &autofill::Suggestion::popup_item_id);
  unlock_iter->is_loading = is_loading;
  return suggestions;
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
    : suggestion_generator_(password_manager_driver, password_client),
      password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      password_client_(password_client) {}

PasswordAutofillManager::~PasswordAutofillManager() {
  CancelBiometricReauthIfOngoing();
}

absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordAutofillManager::GetDriver() {
  return password_manager_driver_.get();
}

void PasswordAutofillManager::OnPopupShown() {}

void PasswordAutofillManager::OnPopupHidden() {}

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
                     weak_ptr_factory_.GetWeakPtr(), unlock_item));
}

void PasswordAutofillManager::DidAcceptSuggestion(
    const autofill::Suggestion& suggestion,
    const SuggestionPosition& position) {
  using metrics_util::PasswordDropdownSelectedOption;
  bool should_hide_popup = true;
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
      should_hide_popup = false;
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->SelectPasskey(GetBackendId(suggestion),
                          base::BindOnce(&PasswordAutofillManager::HidePopup,
                                         weak_ptr_factory_.GetWeakPtr()));
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
      std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
          password_client_->GetDeviceAuthenticator();
      // Note: this is currently only implemented on Android, Mac and Windows.
      // For other platforms, the `authenticator` will be null.
      if (!password_client_->CanUseBiometricAuthForFilling(
              authenticator.get())) {
        bool success = FillSuggestion(
            GetUsernameFromSuggestion(suggestion.main_text.value),
            suggestion.popup_item_id);
        DCHECK(success);
      } else {
        authenticator_ = std::move(authenticator);

        std::u16string message;
        auto on_reath_complete = base::BindOnce(
            &PasswordAutofillManager::OnBiometricReauthCompleted,
            weak_ptr_factory_.GetWeakPtr(), suggestion.main_text.value,
            suggestion.popup_item_id);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        const std::u16string origin =
            base::UTF8ToUTF16(GetShownOrigin(url::Origin::Create(
                password_manager_driver_->GetLastCommittedURL())));
        message = l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif
        authenticator_->AuthenticateWithMessage(
            message, metrics_util::TimeCallback(
                         std::move(on_reath_complete),
                         "PasswordManager.PasswordFilling.AuthenticationTime"));
      }
      break;
  }

  if (should_hide_popup) {
    autofill_client_->HideAutofillPopup(
        autofill::PopupHidingReason::kAcceptSuggestion);
  }
}

void PasswordAutofillManager::DidPerformButtonActionForSuggestion(
    const autofill::Suggestion&) {
  // Button actions do currently not exist for password entries.
  NOTREACHED();
}

bool PasswordAutofillManager::RemoveSuggestion(
    const autofill::Suggestion& suggestion) {
  // Password suggestions cannot be deleted this way.
  // See http://crbug.com/329038#c15
  return false;
}

void PasswordAutofillManager::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
}

autofill::FillingProduct PasswordAutofillManager::GetMainFillingProduct()
    const {
  return autofill::FillingProduct::kPassword;
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
  UpdatePopup(suggestion_generator_.GetSuggestionsForDomain(
      fill_data, page_favicon_, std::u16string(),
      OffersGeneration(false), ShowPasswordSuggestions(true),
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
    autofill::FieldRendererId element_id,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    base::i18n::TextDirection text_direction,
    const std::u16string& typed_username,
    ShowWebAuthnCredentials show_webauthn_credentials,
    const gfx::RectF& bounds) {
  if (autofill::IsAutofillManuallyTriggered(trigger_source)) {
    if (!manual_fallback_flow_) {
      manual_fallback_flow_ = std::make_unique<PasswordManualFallbackFlow>(
          password_manager_driver_, autofill_client_, password_client_,
          password_client_->GetPasswordManager()->GetPasswordFormCache(),
          std::make_unique<SavedPasswordsPresenter>(
              password_client_->GetAffiliationService(),
              password_client_->GetProfilePasswordStore(),
              password_client_->GetAccountPasswordStore()));
    }
    manual_fallback_flow_->RunFlow(element_id, bounds, text_direction);
    return;
  }
  bool autofill_available =
      ShowPopup(bounds, text_direction,
                suggestion_generator_.GetSuggestionsForDomain(
                    fill_data_.get(), page_favicon_, typed_username,
                    OffersGeneration(false), ShowPasswordSuggestions(true),
                    show_webauthn_credentials));

  password_manager_driver_->SetSuggestionAvailability(
      element_id,
      autofill_available
          ? autofill::mojom::AutofillSuggestionAvailability::kAutofillAvailable
          : autofill::mojom::AutofillSuggestionAvailability::kNoSuggestions);
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestions(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  return ShowPopup(bounds, text_direction,
                   suggestion_generator_.GetSuggestionsForDomain(
                       fill_data_.get(), page_favicon_, std::u16string(),
                       OffersGeneration(false), ShowPasswordSuggestions(true),
                       ShowWebAuthnCredentials(false)));
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestionsWithGeneration(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    bool show_password_suggestions) {
  return ShowPopup(bounds, text_direction,
                   suggestion_generator_.GetSuggestionsForDomain(
                       fill_data_.get(), page_favicon_, std::u16string(),
                       OffersGeneration(true),
                       ShowPasswordSuggestions(show_password_suggestions),
                       ShowWebAuthnCredentials(false)));
}

void PasswordAutofillManager::DidNavigateMainFrame() {
  fill_data_.reset();
  CancelBiometricReauthIfOngoing();
  favicon_tracker_.TryCancelAll();
  page_favicon_ = gfx::Image();
  manual_fallback_flow_.reset();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    const std::u16string& username) {
  return FillSuggestion(username, autofill::PopupItemId::kPasswordEntry);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    const std::u16string& username) {
  return PreviewSuggestion(username, autofill::PopupItemId::kPasswordEntry);
}

void PasswordAutofillManager::SetManualFallbackFlowForTest(
    std::unique_ptr<PasswordSuggestionFlow> manual_fallback_flow) {
  manual_fallback_flow_.swap(manual_fallback_flow);
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

void PasswordAutofillManager::LogMetricsForSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) const {
  metrics_util::PasswordDropdownState dropdown_state =
      metrics_util::PasswordDropdownState::kStandard;
  for (const auto& suggestion : suggestions) {
    switch (suggestion.popup_item_id) {
      case autofill::PopupItemId::kGeneratePasswordEntry:
        // TODO(crbug.com/40122999): Revisit metrics for the "opt in and
        // generate" button.
      case autofill::PopupItemId::kPasswordAccountStorageOptInAndGenerate:
        dropdown_state = metrics_util::PasswordDropdownState::kStandardGenerate;
        break;
      default:
        break;
    }
  }
  metrics_util::LogPasswordDropdownShown(dropdown_state);
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
  // TODO(crbug.com/991253): Set the right `form_control_ax_id`.
  last_popup_open_args_ = autofill::AutofillClient::PopupOpenArgs(
      bounds, text_direction, suggestions,
      autofill::AutofillSuggestionTriggerSource::kPasswordManager,
      /*form_control_ax_id=*/0);
  autofill_client_->ShowAutofillPopup(last_popup_open_args_,
                                      weak_ptr_factory_.GetWeakPtr());
  return true;
}

void PasswordAutofillManager::UpdatePopup(
    std::vector<autofill::Suggestion> suggestions) {
  if (!password_manager_driver_->CanShowAutofillUi())
    return;
  if (!ContainsOtherThanManagePasswords(suggestions)) {
    autofill_client_->HideAutofillPopup(
        autofill::PopupHidingReason::kNoSuggestions);
    return;
  }
  autofill_client_->UpdatePopup(
      suggestions, autofill::FillingProduct::kPassword,
      autofill::AutofillSuggestionTriggerSource::kPasswordManager);
  last_popup_open_args_.suggestions = std::move(suggestions);
}

bool PasswordAutofillManager::FillSuggestion(
    const std::u16string& username,
    autofill::PopupItemId popup_item_id) {
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ &&
      GetPasswordAndMetadataForUsername(username, popup_item_id, *fill_data_,
                                        &password_and_meta_data)) {
    bool is_android_credential =
        affiliations::FacetURI::FromPotentiallyInvalidSpec(
            password_and_meta_data.realm)
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
  // (e.g. Keychain access on Mac OS), use `password_manager_client_` here to
  // fetch the actual password. See crbug.com/178358 for more context.

  bool item_uses_account_store =
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
    PasswordManagerClient::ReauthSucceeded reauth_succeeded) {
  autofill_client_->ShowAutofillPopup(last_popup_open_args_,
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
  UpdatePopup(
      SetUnlockLoadingState(std::move(last_popup_open_args_).suggestions,
                            unlock_item, IsLoading(false)));
  // Resets the popup arguments until the next ShowPopup() call.
  last_popup_open_args_ = {};
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
  authenticator_->Cancel();
  authenticator_.reset();
}

void PasswordAutofillManager::HidePopup() {
  autofill_client_->HideAutofillPopup(
      autofill::PopupHidingReason::kAcceptSuggestion);
}

}  //  namespace password_manager
