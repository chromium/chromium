// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
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
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_flow.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::Suggestion;
using autofill::password_generation::PasswordGenerationType;
using IsLoading = autofill::Suggestion::IsLoading;

// This covers the 95th percentile on desktop platforms. See
// `PasswordManager.PasskeyRetrievalWaitDuration` metric.
constexpr base::TimeDelta kWaitForPasskeysDelay = base::Milliseconds(4000);

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
  return std::ranges::any_of(suggestions, [](const auto& s) {
    return s.type != autofill::SuggestionType::kAllSavedPasswordsEntry;
  });
}

std::string GetGuidFromSuggestion(const Suggestion& suggestion) {
  return std::holds_alternative<Suggestion::Guid>(suggestion.payload)
             ? suggestion.GetPayload<Suggestion::Guid>().value()
             : std::string();
}

std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion) {
  auto modifier_fun = [&selected_suggestion](auto& suggestion) {
    using enum Suggestion::Acceptability;
    if (suggestion == selected_suggestion) {
      suggestion.acceptability = kUnacceptable;
      suggestion.is_loading = IsLoading(true);
    } else {
      suggestion.acceptability = kUnacceptableWithDeactivatedStyle;
    }
  };
  std::ranges::for_each(current_suggestions, modifier_fun);
  return current_suggestions;
}

bool AreNewSuggestionsTheSame(
    const std::vector<autofill::Suggestion>& new_suggestions,
    const std::vector<autofill::Suggestion>& old_suggestions) {
  return std::ranges::equal(
      new_suggestions, old_suggestions, [](const auto& lhs, const auto& rhs) {
        return lhs.main_text == rhs.main_text && lhs.type == rhs.type &&
               lhs.icon == rhs.icon && lhs.payload == rhs.payload;
      });
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client,
    PasswordManagerClient* password_client)
    : suggestion_generator_(password_manager_driver,
                            password_client,
                            autofill_client),
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

void PasswordAutofillManager::ShowSuggestions(
    const autofill::TriggeringField& triggering_field) {
  // TODO: crbug.com/410743802 - Implement.
}

#if BUILDFLAG(IS_ANDROID)
void PasswordAutofillManager::ShowKeyboardReplacingSurface(
    const autofill::PasswordSuggestionRequest& request) {
  // TODO: crbug.com/410743802 - Implement.
}
#endif  // BUILDFLAG(IS_ANDROID)

std::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
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
      suggestion.type == autofill::SuggestionType::kGeneratePasswordEntry ||
      suggestion.type ==
          autofill::SuggestionType::kWebauthnSignInWithAnotherDevice) {
    return;
  }

  PreviewSuggestion(GetUsernameFromSuggestion(suggestion.main_text.value),
                    suggestion.type);
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
    case autofill::SuggestionType::kWebauthnCredential:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kWebAuthn,
          password_client_->IsOffTheRecord());
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->SelectPasskey(GetGuidFromSuggestion(suggestion),
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
    case autofill::SuggestionType::kPendingStateSignin:
      password_client_->TriggerSignIn(
          signin_metrics::AccessPoint::kAutofillDropdown);
      break;
    case autofill::SuggestionType::kIdentityCredential: {
      if (const autofill::IdentityCredentialDelegate*
              identity_credential_delegate =
                  autofill_client_->GetIdentityCredentialDelegate()) {
        identity_credential_delegate->NotifySuggestionAccepted(
            suggestion, /*show_modal=*/false,
            base::BindOnce(
                [](base::WeakPtr<PasswordAutofillManager> manager,
                   bool accepted) {
                  if (!manager) {
                    return;
                  }
                  // When notifying the delegate, no extra permission prompts
                  // are requested. The pop-up in its loading state is hidden
                  // regardless of the accepted result.
                  manager->HidePopup();
                },
                weak_ptr_factory_.GetWeakPtr()));
      }
      UpdatePopup(PrepareLoadingStateSuggestions(
          std::move(last_popup_open_args_).suggestions, suggestion));
      break;
    }
    default:
      metrics_util::LogPasswordDropdownItemSelected(
          PasswordDropdownSelectedOption::kPassword,
          password_client_->IsOffTheRecord());

      const autofill::PasswordAndMetadata* password_credential =
          GetPasswordAndMetadataForUsername(
              GetUsernameFromSuggestion(suggestion.main_text.value),
              suggestion.type);
      if (!password_credential) {
        // Navigation happened before suggestion acceptance.
        return;
      }
      if (password_credential->is_grouped_affiliation) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
        cross_domain_confirmation_controller_ =
            password_client_->ShowCrossDomainConfirmationPopup(
                last_popup_open_args_.element_bounds,
                last_popup_open_args_.text_direction,
                /*domain=*/password_manager_driver_->GetLastCommittedURL(),
                /*password_hostname=*/
                password_manager_util::GetHumanReadableRealm(
                    password_credential->realm),
                /*show_warning_text=*/true,
                /*confirmation_callback=*/
                base::BindOnce(&PasswordAutofillManager::
                                   OnPasswordCredentialSuggestionAccepted,
                               weak_ptr_factory_.GetWeakPtr(),
                               *password_credential));
#endif
      } else {
        OnPasswordCredentialSuggestionAccepted(*password_credential);
      }
  }

  bool enter_loading_state =
      password_client_
          ->GetWebAuthnCredentialsDelegateForDriver(password_manager_driver_)
          ->HasPendingPasskeySelection() ||
      suggestion.type == autofill::SuggestionType::kIdentityCredential;
  if (!enter_loading_state) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kAcceptSuggestion);
  }
}

void PasswordAutofillManager::DidPerformButtonActionForSuggestion(
    const Suggestion&,
    const autofill::SuggestionButtonAction&) {
  // Button actions do currently not exist for password entries.
  NOTREACHED();
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
  UpdatePopup(suggestion_generator_.GetSuggestionsForDomain(
      fill_data, page_favicon_, std::u16string(), OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false),
      ShowIdentityCredentials(false)));
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
    ShowIdentityCredentials show_identity_credentials,
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

  if (trigger_source == autofill::AutofillSuggestionTriggerSource::
                            kPasswordManagerProcessedFocusedField &&
      base::FeatureList::IsEnabled(
          features::kDelaySuggestionsOnAutofocusWaitingForPasskeys) &&
      show_webauthn_credentials.value()) {
    if (WebAuthnCredentialsDelegate* delegate =
            password_client_->GetWebAuthnCredentialsDelegateForDriver(
                password_manager_driver_)) {
      auto maybe_passkeys = delegate->GetPasskeys();
      if (!maybe_passkeys.has_value() &&
          maybe_passkeys.error() ==
              WebAuthnCredentialsDelegate::PasskeysUnavailableReason::
                  kNotReceived) {
        if (!wait_for_passkeys_timer_.IsRunning()) {
          base::OnceClosure continue_callback = base::BindOnce(
              &PasswordAutofillManager::ContinueShowingPasswordSuggestions,
              GetWeakPtr(), element_id, text_direction, typed_username,
              show_webauthn_credentials, show_identity_credentials, bounds);
          wait_for_passkeys_timer_.Start(FROM_HERE, kWaitForPasskeysDelay,
                                         std::move(continue_callback));

          // If passkeys become available before the timer expires, this closure
          // runs. It is similar to `continue_callback` but it has to check that
          // the timer is still running, and cancel it if so.
          base::OnceClosure passkeys_available_callback = base::BindOnce(
              [](base::WeakPtr<PasswordAutofillManager> manager,
                 autofill::FieldRendererId element_id,
                 base::i18n::TextDirection text_direction,
                 std::u16string typed_username,
                 ShowWebAuthnCredentials show_webauthn_credentials,
                 ShowIdentityCredentials show_identity_credentials,
                 gfx::RectF bounds) {
                if (!manager) {
                  return;
                }
                if (manager->wait_for_passkeys_timer_.IsRunning()) {
                  manager->wait_for_passkeys_timer_.Stop();
                  manager->ContinueShowingPasswordSuggestions(
                      element_id, text_direction, typed_username,
                      show_webauthn_credentials, show_identity_credentials,
                      bounds);
                }
              },
              GetWeakPtr(), element_id, text_direction, typed_username,
              show_webauthn_credentials, show_identity_credentials, bounds);
          delegate->RequestNotificationWhenPasskeysReady(
              std::move(passkeys_available_callback));
        }
        return;
      }
    }
  }

  wait_for_passkeys_timer_.Stop();

  ContinueShowingPasswordSuggestions(element_id, text_direction, typed_username,
                                     show_webauthn_credentials,
                                     show_identity_credentials, bounds);
}

void PasswordAutofillManager::ContinueShowingPasswordSuggestions(
    autofill::FieldRendererId element_id,
    base::i18n::TextDirection text_direction,
    const std::u16string& typed_username,
    ShowWebAuthnCredentials show_webauthn_credentials,
    ShowIdentityCredentials show_identity_credentials,
    const gfx::RectF& bounds) {
  bool autofill_available =
      ShowPopup(bounds, text_direction,
                suggestion_generator_.GetSuggestionsForDomain(
                    fill_data_.get(), page_favicon_, typed_username,
                    OffersGeneration(false), ShowPasswordSuggestions(true),
                    show_webauthn_credentials, show_identity_credentials),
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
  return ShowPopup(
      bounds, text_direction,
      suggestion_generator_.GetSuggestionsForDomain(
          fill_data_.get(), page_favicon_, std::u16string(),
          OffersGeneration(false), ShowPasswordSuggestions(true),
          ShowWebAuthnCredentials(false), ShowIdentityCredentials(false)),
      /*is_for_webauthn_request=*/false);
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestionsWithGeneration(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    bool show_password_suggestions) {
  return ShowPopup(
      bounds, text_direction,
      suggestion_generator_.GetSuggestionsForDomain(
          fill_data_.get(), page_favicon_, std::u16string(),
          OffersGeneration(true),
          ShowPasswordSuggestions(show_password_suggestions),
          ShowWebAuthnCredentials(false), ShowIdentityCredentials(false)),
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  cross_domain_confirmation_controller_.reset();
#endif
  wait_for_passkeys_timer_.Stop();
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

void PasswordAutofillManager::FillSuggestion(
    const autofill::PasswordAndMetadata& password_and_metadata) {
  bool is_android_credential =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          password_and_metadata.realm)
          .IsValidAndroidFacetURI();
  metrics_util::LogFilledPasswordFromAndroidApp(is_android_credential);
  // Emit UMA if grouped affiliation match was available for the user.
  if (fill_data_->preferred_login.is_grouped_affiliation ||
      std::ranges::find_if(fill_data_->additional_logins,
                           [](const autofill::PasswordAndMetadata& login) {
                             return login.is_grouped_affiliation;
                           }) != fill_data_->additional_logins.end()) {
    metrics_util::LogFillSuggestionGroupedMatchAccepted(
        password_and_metadata.is_grouped_affiliation);
  }
  password_manager_driver_->FillSuggestion(password_and_metadata.username_value,
                                           password_and_metadata.password_value,
                                           base::DoNothing());
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
  if (const autofill::PasswordAndMetadata* password_and_metadata =
          GetPasswordAndMetadataForUsername(username, type)) {
    password_manager_driver_->PreviewSuggestion(
        username, password_and_metadata->password_value);
    return true;
  }
  return false;
}

const autofill::PasswordAndMetadata*
PasswordAutofillManager::GetPasswordAndMetadataForUsername(
    const std::u16string& current_username,
    autofill::SuggestionType type) {
  if (!fill_data_) {
    return nullptr;
  }

  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use `password_manager_client_` here to
  // fetch the actual password. See crbug.com/178358 for more context.

  bool item_uses_account_store =
      type == autofill::SuggestionType::kAccountStoragePasswordEntry;

  // Look for any suitable matches to current field text.
  if (fill_data_->preferred_login.username_value == current_username &&
      fill_data_->preferred_login.uses_account_store ==
          item_uses_account_store) {
    return &fill_data_->preferred_login;
  }

  // Scan additional logins for a match.
  auto iter = std::ranges::find_if(
      fill_data_->additional_logins,
      [&](const autofill::PasswordAndMetadata& login) {
        return current_username == login.username_value &&
               item_uses_account_store == login.uses_account_store;
      });
  if (iter != fill_data_->additional_logins.end()) {
    return &*iter;
  }

  return nullptr;
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

void PasswordAutofillManager::OnBiometricReauthCompleted(
    const autofill::PasswordAndMetadata& password_and_metadata,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded) {
    return;
  }
  FillSuggestion(password_and_metadata);
}

void PasswordAutofillManager::OnPasswordCredentialSuggestionAccepted(
    const autofill::PasswordAndMetadata& password_and_metadata) {
  CancelBiometricReauthIfOngoing();
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      password_client_->GetDeviceAuthenticator();
  // Note: this is currently only implemented on Android, Mac and Windows.
  // For other platforms, the `authenticator` will be null.
  if (!password_client_->IsReauthBeforeFillingRequired(authenticator.get())) {
    FillSuggestion(password_and_metadata);
    return;
  }
  authenticator_ = std::move(authenticator);

  std::u16string message;
  auto on_reath_complete =
      base::BindOnce(&PasswordAutofillManager::OnBiometricReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), password_and_metadata);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  const std::u16string origin = base::UTF8ToUTF16(GetShownOrigin(
      url::Origin::Create(password_manager_driver_->GetLastCommittedURL())));
  message =
      l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  authenticator_->AuthenticateWithMessage(
      message, metrics_util::TimeCallbackMediumTimes(
                   std::move(on_reath_complete),
                   "PasswordManager.PasswordFilling.AuthenticationTime2"));
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

void PasswordAutofillManager::FocusedInputChanged() {
  wait_for_passkeys_timer_.Stop();
}

}  //  namespace password_manager
