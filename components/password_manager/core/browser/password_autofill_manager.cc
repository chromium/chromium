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
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
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
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
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
using autofill::SuggestionType;
using autofill::password_generation::PasswordGenerationType;
using SuggestionMetadata =
    autofill::AutofillSuggestionDelegate::SuggestionMetadata;
using IsLoading = autofill::Suggestion::IsLoading;

bool IsSuggestionHandledInPasswordManager(SuggestionType type) {
  switch (type) {
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return true;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
      return false;
  }
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

// Check whether the `new_suggestions` have a suggestion with the Pending Signin
// state that is not present in the `old_suggestions`. This assumes that a
// suggestion of type `kPendingStateSignin` is only present once in the list (or
// that multiple instances are the same).
bool IsNewSigninPendingSuggestion(
    const std::vector<autofill::Suggestion>& new_suggestions,
    const std::vector<autofill::Suggestion>& old_suggestions) {
  auto new_it = std::ranges::find_if(
      new_suggestions, [](const autofill::Suggestion& suggestion) {
        return suggestion.type == autofill::SuggestionType::kPendingStateSignin;
      });
  if (new_it == new_suggestions.end()) {
    return false;
  }

  return std::ranges::find(old_suggestions, *new_it) == old_suggestions.end();
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

#if BUILDFLAG(IS_ANDROID)
void PasswordAutofillManager::ShowKeyboardReplacingSurface(
    const autofill::PasswordSuggestionRequest& request) {
  password_client_->ShowKeyboardReplacingSurface(password_manager_driver_,
                                                 request);
}
#endif  // BUILDFLAG(IS_ANDROID)

std::optional<autofill::Suggestion>
PasswordAutofillManager::GetWebauthnSignInWithAnotherDeviceSuggestion() const {
  return suggestion_generator_.GetWebauthnSignInWithAnotherDeviceSuggestion();
}

std::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordAutofillManager::GetDriver() {
  return password_manager_driver_.get();
}

void PasswordAutofillManager::OnSuggestionsShown(
    base::span<const Suggestion> suggestions) {}

void PasswordAutofillManager::OnSuggestionsHidden() {
  password_client_->GetUndoPasswordChangeController()->OnSuggestionsHidden();
  metrics_util::LogPasswordDropdownHidden();
}

void PasswordAutofillManager::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();
  if (suggestion.type == autofill::SuggestionType::kAllSavedPasswordsEntry ||
      suggestion.type == autofill::SuggestionType::kGeneratePasswordEntry ||
      suggestion.type ==
          autofill::SuggestionType::kWebauthnSignInWithAnotherDevice ||
      suggestion.type == autofill::SuggestionType::kTroubleSigningInEntry) {
    return;
  }
  if (suggestion.type == autofill::SuggestionType::kBackupPasswordEntry) {
    const auto payload =
        suggestion
            .GetPayload<autofill::Suggestion::PasswordSuggestionDetails>();
    CHECK(payload.backup_password);
    password_manager_driver_->PreviewSuggestion(
        payload.username, payload.backup_password.value());
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
      metrics_util::LogPasswordSuggestionSelected(
          PasswordDropdownSelectedOption::kGenerate,
          password_client_->IsOffTheRecord());
      break;
    case autofill::SuggestionType::kAllSavedPasswordsEntry:
      password_client_->NavigateToManagePasswordsPage(
          ManagePasswordsReferrer::kPasswordDropdown);
      metrics_util::LogPasswordSuggestionSelected(
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
      metrics_util::LogPasswordSuggestionSelected(
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
      metrics_util::LogPasswordSuggestionSelected(
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
    case autofill::SuggestionType::kBackupPasswordEntry: {
      metrics_util::LogPasswordSuggestionSelected(
          PasswordDropdownSelectedOption::kBackupPassword,
          password_client_->IsOffTheRecord());
      // The payload is set during suggestion generation and contains the backup
      // password in its password field.
      auto payload =
          suggestion
              .GetPayload<autofill::Suggestion::PasswordSuggestionDetails>();
      OnPasswordCredentialSuggestionAccepted(
          base::BindOnce(&PasswordAutofillManager::FillBackupSuggestion,
                         weak_ptr_factory_.GetWeakPtr(), payload));
      break;
    }
    case autofill::SuggestionType::kTroubleSigningInEntry: {
      metrics_util::LogPasswordSuggestionSelected(
          PasswordDropdownSelectedOption::kTroubleSigningIn,
          password_client_->IsOffTheRecord());
      auto payload =
          suggestion
              .GetPayload<autofill::Suggestion::PasswordSuggestionDetails>();
      password_client_->GetUndoPasswordChangeController()
          ->OnTroubleSigningInClicked(payload);

      UpdatePopup(
          suggestion_generator_.GetProactiveRecoverySuggestions(payload));
      return;
    }
    default: {
      metrics_util::LogPasswordSuggestionSelected(
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
      auto fill_suggestion_callback =
          base::BindOnce(&PasswordAutofillManager::FillSuggestion,
                         weak_ptr_factory_.GetWeakPtr(), *password_credential);
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
                               std::move(fill_suggestion_callback)));
#endif
      } else {
        OnPasswordCredentialSuggestionAccepted(
            std::move(fill_suggestion_callback));
      }
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
  // The value was likely filled on page load, progress the state of the
  // recovery flow.
  if (!fill_data.wait_for_username) {
    password_client_->GetUndoPasswordChangeController()->OnSuggestionSelected(
        fill_data.preferred_login);
  }

  if (!autofill_client_ || autofill_client_->GetAutofillSuggestions().empty()) {
    return;
  }
  UpdatePopup(GetSuggestions(
      std::u16string(), OffersGeneration(false), ShowPasswordSuggestions(true),
      ShowWebAuthnCredentials(false), ShowIdentityCredentials(false)));
}

void PasswordAutofillManager::DeleteFillData() {
  fill_data_.reset();
  if (autofill_client_) {
    autofill_client_->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kStaleData);
  }
  CancelBiometricReauthIfOngoing();
}

void PasswordAutofillManager::ShowSuggestions(
    const autofill::TriggeringField& field) {
#if !BUILDFLAG(IS_ANDROID)
  if (password_client_->IsActorTaskActive()) {
    // Disables password suggestions if actor is active on the tab.
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  if (autofill::IsPasswordsAutofillManuallyTriggered(field.trigger_source)) {
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
    manual_fallback_flow_->RunFlow(field.element_id, GetBounds(field),
                                   field.text_direction);
    return;
  }

  if (ShouldWaitForPasskeys(field)) {
    WaitForPasskeys(field);
  } else {
    wait_for_passkeys_timer_.Stop();
    ContinueShowingSuggestions(field);
  }
}

void PasswordAutofillManager::SelectSuggestion(const Suggestion& suggestion) {
  CHECK(IsSuggestionHandledInPasswordManager(suggestion.type));
  DidSelectSuggestion(suggestion);
}

void PasswordAutofillManager::AcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  CHECK(IsSuggestionHandledInPasswordManager(suggestion.type));
  DidAcceptSuggestion(suggestion, metadata);
}

bool PasswordAutofillManager::ShouldWaitForPasskeys(
    const autofill::TriggeringField& field) {
  WebAuthnCredentialsDelegate* delegate =
      password_client_->GetWebAuthnCredentialsDelegateForDriver(
          password_manager_driver_);
  if (field.trigger_source != autofill::AutofillSuggestionTriggerSource::
                                  kPasswordManagerProcessedFocusedField ||
      !base::FeatureList::IsEnabled(
          features::kDelaySuggestionsOnAutofocusWaitingForPasskeys) ||
      !field.show_webauthn_credentials || !delegate) {
    return false;  // Delayed passkeys can't be used.
  }

  // Finally, only fetch if Passkeys fetching didn't succeed or fail before.
  auto expected_passkeys = delegate->GetPasskeys();
  return !expected_passkeys.has_value() &&
         expected_passkeys.error() ==
             WebAuthnCredentialsDelegate::PasskeysUnavailableReason::
                 kNotReceived;
}

void PasswordAutofillManager::WaitForPasskeys(
    const autofill::TriggeringField& field) {
  WebAuthnCredentialsDelegate* delegate =
      password_client_->GetWebAuthnCredentialsDelegateForDriver(
          password_manager_driver_);
  CHECK(ShouldWaitForPasskeys(field));
  // Wait only if the timer isn't already running.
  if (wait_for_passkeys_timer_.IsRunning()) {
    return;  // Already waiting.
  }

  base::OnceClosure continue_callback =
      base::BindOnce(&PasswordAutofillManager::ContinueShowingSuggestions,
                     GetWeakPtr(), field);
  wait_for_passkeys_timer_.Start(
      FROM_HERE,
      base::Milliseconds(features::kDelaySuggestionsOnAutofocusTimeout.Get()),
      std::move(continue_callback));

  delegate->RequestNotificationWhenPasskeysReady(base::BindOnce(
      &PasswordAutofillManager::OnPasskeysReady, GetWeakPtr(), field));
}

void PasswordAutofillManager::OnPasskeysReady(
    const autofill::TriggeringField& field) {
  if (!wait_for_passkeys_timer_.IsRunning()) {
    return;  // Request for passkeys not relevant anymore. Ignore the signal.
  }
  wait_for_passkeys_timer_.Stop();
  ContinueShowingSuggestions(field);
}

void PasswordAutofillManager::ContinueShowingSuggestions(
    const autofill::TriggeringField& field) {
  bool autofill_available = ShowPopup(
      GetBounds(field), field.text_direction,
      GetSuggestions(field.typed_username, OffersGeneration(false),
                     ShowPasswordSuggestions(true),
                     ShowWebAuthnCredentials(field.show_webauthn_credentials),
                     ShowIdentityCredentials(true)),
      field.show_webauthn_credentials);

  password_manager_driver_->SetSuggestionAvailability(
      field.element_id,
      autofill_available
          ? autofill::mojom::AutofillSuggestionAvailability::kAutofillAvailable
          : autofill::mojom::AutofillSuggestionAvailability::kNoSuggestions);
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestions(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  return ShowPopup(bounds, text_direction,
                   GetSuggestions(std::u16string(), OffersGeneration(false),
                                  ShowPasswordSuggestions(true),
                                  ShowWebAuthnCredentials(false),
                                  ShowIdentityCredentials(false)),
                   /*is_for_webauthn_request=*/false);
}

bool PasswordAutofillManager::MaybeShowPasswordSuggestionsWithGeneration(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction,
    bool show_password_suggestions) {
  return ShowPopup(
      bounds, text_direction,
      GetSuggestions(std::u16string(), OffersGeneration(true),
                     ShowPasswordSuggestions(show_password_suggestions),
                     ShowWebAuthnCredentials(false),
                     ShowIdentityCredentials(false)),
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

  if (IsNewSigninPendingSuggestion(suggestions,
                                   last_popup_open_args_.suggestions)) {
    signin_metrics::LogSigninPendingOffered(
        signin_metrics::AccessPoint::kAutofillDropdown);
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
  password_client_->GetUndoPasswordChangeController()->OnSuggestionSelected(
      password_and_metadata);
}

void PasswordAutofillManager::FillBackupSuggestion(
    const autofill::Suggestion::PasswordSuggestionDetails& payload) {
  CHECK(payload.backup_password);

  password_manager_driver_->FillSuggestion(
      payload.username, payload.backup_password.value(), base::DoNothing());

  autofill::PasswordAndMetadata password_and_metadata;
  password_and_metadata.username_value = payload.username;
  password_and_metadata.backup_password_value = payload.backup_password;
  password_client_->GetUndoPasswordChangeController()->OnSuggestionSelected(
      password_and_metadata);
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
    base::OnceClosure fill_suggestion_callback,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded) {
    return;
  }
  std::move(fill_suggestion_callback).Run();
}

void PasswordAutofillManager::OnPasswordCredentialSuggestionAccepted(
    base::OnceClosure fill_suggestion_callback) {
  CancelBiometricReauthIfOngoing();
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      password_client_->GetDeviceAuthenticator();
  // Note: this is currently only implemented on Android, Mac and Windows.
  // For other platforms, the `authenticator` will be null.
  if (!password_client_->IsReauthBeforeFillingRequired(authenticator.get())) {
    std::move(fill_suggestion_callback).Run();
    return;
  }
  authenticator_ = std::move(authenticator);

  std::u16string message;
  auto on_reauth_complete = base::BindOnce(
      &PasswordAutofillManager::OnBiometricReauthCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(fill_suggestion_callback));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  const std::u16string origin = base::UTF8ToUTF16(GetShownOrigin(
      url::Origin::Create(password_manager_driver_->GetLastCommittedURL())));
  message =
      l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  authenticator_->AuthenticateWithMessage(
      message, metrics_util::TimeCallbackMediumTimes(
                   std::move(on_reauth_complete),
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

std::vector<autofill::Suggestion> PasswordAutofillManager::GetSuggestions(
    const std::u16string& username_filter,
    OffersGeneration offers_generation,
    ShowPasswordSuggestions show_password_suggestions,
    ShowWebAuthnCredentials show_webauthn_credentials,
    ShowIdentityCredentials show_identity_credentials) {
  std::optional<autofill::PasswordAndMetadata> proactive_recovery_login =
      password_client_->GetUndoPasswordChangeController()
          ->FindLoginWithProactiveRecoveryState(fill_data_.get());
  if (proactive_recovery_login) {
    CHECK(proactive_recovery_login->backup_password_value);

    const auto suggestion_details = Suggestion::PasswordSuggestionDetails(
        proactive_recovery_login->username_value,
        proactive_recovery_login->password_value,
        proactive_recovery_login->backup_password_value.value());
    return suggestion_generator_.GetProactiveRecoverySuggestions(
        suggestion_details);
  }
  return suggestion_generator_.GetSuggestionsForDomain(
      *password_client_->GetUndoPasswordChangeController(), fill_data_.get(),
      page_favicon_, username_filter, offers_generation,
      show_password_suggestions, show_webauthn_credentials,
      show_identity_credentials);
}

gfx::RectF PasswordAutofillManager::GetBounds(
    const autofill::TriggeringField& field) {
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillAndPasswordsInSameSurface)
             ? field.bounds  // Already transformed in ContentAutofillDriver.
             : password_manager_driver_->TransformToRootCoordinates(
                   field.bounds);
}

}  //  namespace password_manager
