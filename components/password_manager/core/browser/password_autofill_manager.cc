// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_flow.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::Suggestion;
using autofill::password_generation::PasswordGenerationType;
using IsLoading = autofill::Suggestion::IsLoading;

// Entry showing the empty state (i.e. no passwords found in account-storage).
Suggestion CreateAccountStorageEmptyEntry() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_NO_ACCOUNT_STORE_MATCHES));
  suggestion.type = autofill::SuggestionType::kPasswordAccountStorageEmpty;
  suggestion.icon = Suggestion::Icon::kEmpty;
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
    base::span<const Suggestion> suggestions) {
  return base::ranges::any_of(suggestions, [](const auto& s) {
    return s.type != autofill::SuggestionType::kAllSavedPasswordsEntry;
  });
}

bool HasLoadingSuggestion(base::span<const Suggestion> suggestions,
                          autofill::SuggestionType item_id) {
  return base::ranges::any_of(suggestions, [&item_id](const auto& suggestion) {
    return suggestion.type == item_id && suggestion.is_loading;
  });
}

std::string GetBackendId(const Suggestion& suggestion) {
  return absl::holds_alternative<Suggestion::BackendId>(suggestion.payload)
             ? suggestion.GetBackendId<Suggestion::Guid>().value()
             : std::string();
}

std::vector<Suggestion> SetUnlockLoadingState(
    std::vector<Suggestion> suggestions,
    autofill::SuggestionType type,
    IsLoading is_loading) {
  using enum autofill::SuggestionType;
  DCHECK(type == kPasswordAccountStorageOptIn ||
         type == kPasswordAccountStorageReSignin ||
         type == kPasswordAccountStorageOptInAndGenerate);
  auto unlock_iter = base::ranges::find(suggestions, type, &Suggestion::type);
  unlock_iter->is_loading = is_loading;
  return suggestions;
}

std::vector<autofill::Suggestion> PrepareLoadingStateSuggestions(
    std::vector<autofill::Suggestion> current_suggestions,
    const autofill::Suggestion& selected_suggestion) {
  auto modifier_fun = [&selected_suggestion](auto& suggestion) {
    if (suggestion == selected_suggestion) {
      suggestion.is_loading = IsLoading(true);
    } else {
      suggestion.apply_deactivated_style = true;
    }
    suggestion.is_acceptable = false;
  };
  base::ranges::for_each(current_suggestions, modifier_fun);
  return current_suggestions;
}

bool AreNewSuggestionsTheSame(
    const std::vector<autofill::Suggestion>& new_suggestions,
    const std::vector<autofill::Suggestion>& old_suggestions) {
  return base::ranges::equal(
      new_suggestions, old_suggestions, [](const auto& lhs, const auto& rhs) {
        return lhs.main_text == rhs.main_text && lhs.type == rhs.type &&
               lhs.icon == rhs.icon && lhs.payload == rhs.payload;
      });
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
      password_client_(password_client),
      manual_fallback_metrics_recorder_(
          std::make_unique<PasswordManualFallbackMetricsRecorder>()) {}

PasswordAutofillManager::~PasswordAutofillManager() {
  CancelBiometricReauthIfOngoing();
  // `manual_fallback_flow_` holds a raw pointer to
  // `manual_fallback_metrics_recorder_`, so the flow should be reset first.
  manual_fallback_flow_.reset();
  manual_fallback_metrics_recorder_.reset();
}

absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordAutofillManager::GetDriver() {
  return password_manager_driver_.get();
}

void PasswordAutofillManager::OnSuggestionsShown(
    base::span<const Suggestion> suggestions) {}

void PasswordAutofillManager::OnSuggestionsHidden() {
  metrics_util::LogPasswordDropdownHidden();
}

void PasswordAutofillManager::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();
  if (suggestion.type == autofill::SuggestionType::kAllSavedPasswordsEntry ||
      suggestion.type ==
          autofill::SuggestionType::kPasswordAccountStorageEmpty ||
      suggestion.type == autofill::SuggestionType::kGeneratePasswordEntry ||
      suggestion.type ==
          autofill::SuggestionType::kPasswordAccountStorageOptIn ||
      suggestion.type ==
          autofill::SuggestionType::kPasswordAccountStorageReSignin ||
      suggestion.type ==
          autofill::SuggestionType::kPasswordAccountStorageOptInAndGenerate ||
      suggestion.type ==
          autofill::SuggestionType::kWebauthnSignInWithAnotherDevice) {
    return;
  }

  PreviewSuggestion(GetUsernameFromSuggestion(suggestion.main_text.value),
                    suggestion.type);
}

void PasswordAutofillManager::OnUnlockItemAccepted(
    autofill::SuggestionType type) {
  using metrics_util::PasswordDropdownSelectedOption;
  using enum autofill::SuggestionType;
  DCHECK(type == kPasswordAccountStorageOptIn ||
         type == kPasswordAccountStorageOptInAndGenerate);

  std::vector<Suggestion> suggestions{
      autofill_client_->GetAutofillSuggestions().begin(),
      autofill_client_->GetAutofillSuggestions().end()};
  UpdatePopup(
      SetUnlockLoadingState(std::move(suggestions), type, IsLoading(true)));
  signin_metrics::ReauthAccessPoint reauth_access_point =
      type == kPasswordAccountStorageOptIn
          ? signin_metrics::ReauthAccessPoint::kAutofillDropdown
          : signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown;
  password_client_->TriggerReauthForPrimaryAccount(
      reauth_access_point,
      base::BindOnce(&PasswordAutofillManager::OnUnlockReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), type));
}

void PasswordAutofillManager::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  using metrics_util::PasswordDropdownSelectedOption;
  switch (suggestion.type) {
    case autofill::SuggestionType::kGeneratePasswordEntry:
      password_client_->GeneratePassword(PasswordGenerationType::kAutomatic);
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kGenerate,
          password_client_->IsOffTheRecord());
      break;
    case autofill::SuggestionType::kAllSavedPasswordsEntry:
    case autofill::SuggestionType::kPasswordAccountStorageEmpty:
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
    case autofill::SuggestionType::kPasswordAccountStorageReSignin:
      password_client_->TriggerSignIn(
          signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kResigninToUnlockAccountStore,
          password_client_->IsOffTheRecord());
      break;
    case autofill::SuggestionType::kPasswordAccountStorageOptIn:
    case autofill::SuggestionType::kPasswordAccountStorageOptInAndGenerate:
      OnUnlockItemAccepted(suggestion.type);
      metrics_util::LogPasswordDropdownItemSelected(
          suggestion.type ==
                  autofill::SuggestionType::kPasswordAccountStorageOptIn
              ? PasswordDropdownSelectedOption::kUnlockAccountStorePasswords
              : PasswordDropdownSelectedOption::kUnlockAccountStoreGeneration,
          password_client_->IsOffTheRecord());
      break;
    case autofill::SuggestionType::kWebauthnCredential:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kWebAuthn,
          password_client_->IsOffTheRecord());
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->SelectPasskey(GetBackendId(suggestion),
                          base::BindOnce(&PasswordAutofillManager::HidePopup,
                                         weak_ptr_factory_.GetWeakPtr()));
      // Disable all entries and set the `selected_suggestion` in loading state.
      // This is used for passkey entries, and it is
      // `WebAuthnCredentialsDelegate`s responsibility to dismiss the popup
      // (e.g. when the passkey response is received from the enclave).
      UpdatePopup(PrepareLoadingStateSuggestions(
          std::move(last_popup_open_args_).suggestions, suggestion));
      break;
    case autofill::SuggestionType::kWebauthnSignInWithAnotherDevice:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kWebAuthnSignInWithAnotherDevice,
          password_client_->IsOffTheRecord());
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->LaunchSecurityKeyOrHybridFlow();
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
      if (!password_client_->IsReauthBeforeFillingRequired(
              authenticator.get())) {
        bool success = FillSuggestion(
            GetUsernameFromSuggestion(suggestion.main_text.value),
            suggestion.type);
        DCHECK(success);
      } else {
        authenticator_ = std::move(authenticator);

        std::u16string message;
        auto on_reath_complete =
            base::BindOnce(&PasswordAutofillManager::OnBiometricReauthCompleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           suggestion.main_text.value, suggestion.type);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
        const std::u16string origin =
            base::UTF8ToUTF16(GetShownOrigin(url::Origin::Create(
                password_manager_driver_->GetLastCommittedURL())));
        message = l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif
        authenticator_->AuthenticateWithMessage(
            message,
            metrics_util::TimeCallbackMediumTimes(
                std::move(on_reath_complete),
                "PasswordManager.PasswordFilling.AuthenticationTime2"));
      }
      break;
  }

  if (!password_client_
           ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
           ->HasPendingPasskeySelection()) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kAcceptSuggestion);
  }
}

void PasswordAutofillManager::DidPerformButtonActionForSuggestion(
    const Suggestion&,
    const autofill::SuggestionButtonAction&) {
  // Button actions do currently not exist for password entries.
  NOTREACHED_IN_MIGRATION();
}

bool PasswordAutofillManager::RemoveSuggestion(const Suggestion& suggestion) {
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
  if (!autofill::IsValidPasswordFormFillData(fill_data)) {
    return;
  }

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

  if (!autofill_client_ || autofill_client_->GetAutofillSuggestions().empty()) {
    return;
  }
  // Only log account-stored passwords if the unlock just happened.
  if (HasLoadingSuggestion(
          autofill_client_->GetAutofillSuggestions(),
          autofill::SuggestionType::kPasswordAccountStorageOptIn)) {
    LogAccountStoredPasswordsCountInFillDataAfterUnlock(fill_data);
  }
  UpdatePopup(suggestion_generator_.GetSuggestionsForDomain(
      fill_data, page_favicon_, std::u16string(), OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false)));
}

void PasswordAutofillManager::OnNoCredentialsFound() {
  if (!autofill_client_ ||
      !HasLoadingSuggestion(
          autofill_client_->GetAutofillSuggestions(),
          autofill::SuggestionType::kPasswordAccountStorageOptIn)) {
    return;
  }
  metrics_util::LogPasswordsCountFromAccountStoreAfterUnlock(
      /*account_store_passwords_count=*/0);
  UpdatePopup({CreateAccountStorageEmptyEntry()});
}

void PasswordAutofillManager::DeleteFillData() {
  fill_data_.reset();
  if (autofill_client_) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kStaleData);
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
          manual_fallback_metrics_recorder_.get(),
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
                    show_webauthn_credentials),
                show_webauthn_credentials.value());

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
                       ShowWebAuthnCredentials(false)),
                   /*is_for_webauthn_request=*/false);
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
                       ShowWebAuthnCredentials(false)),
                   /*is_for_webauthn_request=*/false);
}

void PasswordAutofillManager::DidNavigateMainFrame() {
  fill_data_.reset();
  CancelBiometricReauthIfOngoing();
  favicon_tracker_.TryCancelAll();
  page_favicon_ = gfx::Image();
  // `manual_fallback_flow_` holds a raw pointer to
  // `manual_fallback_metrics_recorder_`, so the flow should be reset first.
  manual_fallback_flow_.reset();
  manual_fallback_metrics_recorder_ =
      std::make_unique<PasswordManualFallbackMetricsRecorder>();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    const std::u16string& username) {
  return FillSuggestion(username, autofill::SuggestionType::kPasswordEntry);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    const std::u16string& username) {
  return PreviewSuggestion(username, autofill::SuggestionType::kPasswordEntry);
}

void PasswordAutofillManager::SetManualFallbackFlowForTest(
    std::unique_ptr<PasswordSuggestionFlow> manual_fallback_flow) {
  manual_fallback_flow_.swap(manual_fallback_flow);
}

base::WeakPtr<PasswordAutofillManager> PasswordAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

bool PasswordAutofillManager::ShowPopup(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    bool is_for_webauthn_request) {
  if (!password_manager_driver_->CanShowAutofillUi()) {
    return false;
  }
  if (!ContainsOtherThanManagePasswords(suggestions)) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kNoSuggestions);
    return false;
  }
  if (!password_client_
           ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
           ->HasPendingPasskeySelection() ||
      !AreNewSuggestionsTheSame(suggestions,
                                last_popup_open_args_.suggestions)) {
    metrics_util::LogPasswordDropdownShown(suggestions);
    metrics_util::MaybeLogMetricsForPasswordAndWebauthnCounts(
        suggestions, is_for_webauthn_request);
    // TODO(crbug.com/41474723): Set the right `form_control_ax_id`.
    last_popup_open_args_ = autofill::AutofillClient::PopupOpenArgs(
        bounds, text_direction, suggestions,
        autofill::AutofillSuggestionTriggerSource::kPasswordManager,
        /*form_control_ax_id=*/0, autofill::PopupAnchorType::kField);
  }
  autofill_client_->ShowAutofillSuggestions(last_popup_open_args_,
                                            weak_ptr_factory_.GetWeakPtr());
  return true;
}

void PasswordAutofillManager::UpdatePopup(std::vector<Suggestion> suggestions) {
  if (!password_manager_driver_->CanShowAutofillUi()) {
    return;
  }
  if (!ContainsOtherThanManagePasswords(suggestions)) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kNoSuggestions);
    return;
  }
  autofill_client_->UpdateAutofillSuggestions(
      suggestions, autofill::FillingProduct::kPassword,
      autofill::AutofillSuggestionTriggerSource::kPasswordManager);
  last_popup_open_args_.suggestions = std::move(suggestions);
}

bool PasswordAutofillManager::FillSuggestion(const std::u16string& username,
                                             autofill::SuggestionType type) {
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ && GetPasswordAndMetadataForUsername(
                        username, type, *fill_data_, &password_and_meta_data)) {
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

bool PasswordAutofillManager::PreviewSuggestion(const std::u16string& username,
                                                autofill::SuggestionType type) {
  if (type == autofill::SuggestionType::kWebauthnCredential) {
    password_manager_driver_->PreviewSuggestion(username, /*password=*/u"");
    return true;
  }
  if (password_client_->GetPasswordFeatureManager()
          ->IsBiometricAuthenticationBeforeFillingEnabled()) {
    return false;
  }
  autofill::PasswordAndMetadata password_and_meta_data;
  if (fill_data_ && GetPasswordAndMetadataForUsername(
                        username, type, *fill_data_, &password_and_meta_data)) {
    password_manager_driver_->PreviewSuggestion(
        username, password_and_meta_data.password_value);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::GetPasswordAndMetadataForUsername(
    const std::u16string& current_username,
    autofill::SuggestionType type,
    const autofill::PasswordFormFillData& fill_data,
    autofill::PasswordAndMetadata* password_and_meta_data) {
  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use `password_manager_client_` here to
  // fetch the actual password. See crbug.com/178358 for more context.

  bool item_uses_account_store =
      type == autofill::SuggestionType::kAccountStoragePasswordEntry;

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
  if (!password_client_) {
    return;
  }
  favicon::GetFaviconImageForPageURL(
      password_client_->GetFaviconService(), url,
      favicon_base::IconType::kFavicon,
      base::BindOnce(&PasswordAutofillManager::OnFaviconReady,
                     weak_ptr_factory_.GetWeakPtr()),
      &favicon_tracker_);
}

void PasswordAutofillManager::OnFaviconReady(
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty()) {
    page_favicon_ = result.image;
  }
}

void PasswordAutofillManager::OnUnlockReauthCompleted(
    autofill::SuggestionType type,
    PasswordManagerClient::ReauthSucceeded reauth_succeeded) {
  autofill_client_->ShowAutofillSuggestions(last_popup_open_args_,
                                            weak_ptr_factory_.GetWeakPtr());
  autofill_client_->PinAutofillSuggestions();
  if (reauth_succeeded) {
    if (type ==
        autofill::SuggestionType::kPasswordAccountStorageOptInAndGenerate) {
      password_client_->GeneratePassword(PasswordGenerationType::kAutomatic);
      autofill_client_->HideAutofillSuggestions(
          autofill::SuggestionHidingReason::kAcceptSuggestion);
    }
    return;
  }
  UpdatePopup(SetUnlockLoadingState(
      std::move(last_popup_open_args_).suggestions, type, IsLoading(false)));
  // Resets the popup arguments until the next ShowPopup() call.
  last_popup_open_args_ = {};
}

void PasswordAutofillManager::OnBiometricReauthCompleted(
    const std::u16string& value,
    autofill::SuggestionType type,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded) {
    return;
  }
  bool success = FillSuggestion(GetUsernameFromSuggestion(value), type);
  DCHECK(success);
}

void PasswordAutofillManager::CancelBiometricReauthIfOngoing() {
  if (!authenticator_) {
    return;
  }
  authenticator_->Cancel();
  authenticator_.reset();
}

void PasswordAutofillManager::HidePopup() {
  autofill_client_->HideAutofillSuggestions(
      autofill::SuggestionHidingReason::kAcceptSuggestion);
}

}  //  namespace password_manager
