// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include "base/functional/bind.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "url/gurl.h"

namespace password_manager {

namespace {
using autofill::Suggestion;

// If `label` was made for an empty username, then return the empty string,
// otherwise return `label`.
std::u16string GetUsernameFromLabel(const std::u16string& label) {
  return label == l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? std::u16string()
             : label;
}
}  // namespace

PasswordManualFallbackFlow::PasswordManualFallbackFlow(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client,
    PasswordManagerClient* password_client,
    const PasswordFormCache* password_form_cache,
    std::unique_ptr<SavedPasswordsPresenter> passwords_presenter)
    : suggestion_generator_(password_manager_driver, password_client),
      password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      password_client_(password_client),
      password_form_cache_(password_form_cache),
      passwords_presenter_(std::move(passwords_presenter)) {
  passwords_presenter_observation_.Observe(passwords_presenter_.get());
  passwords_presenter_->Init();

  const GURL origin_as_gurl = password_manager_driver_->GetLastCommittedURL();
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(origin_as_gurl), origin_as_gurl);
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest, password_client, /*should_migrate_http_passwords=*/false);
  form_fetcher_->set_filter_grouped_credentials(false);
  form_fetcher_->AddConsumer(this);
  form_fetcher_->Fetch();
}

PasswordManualFallbackFlow::~PasswordManualFallbackFlow() {
  CancelBiometricReauthIfOngoing();
  form_fetcher_->RemoveConsumer(this);
}

// static
bool PasswordManualFallbackFlow::SupportsSuggestionType(
    autofill::SuggestionType type) {
  switch (type) {
    case autofill::SuggestionType::kPasswordEntry:
    case autofill::SuggestionType::kPasswordFieldByFieldFilling:
    case autofill::SuggestionType::kFillPassword:
    case autofill::SuggestionType::kViewPasswordDetails:
    case autofill::SuggestionType::kAllSavedPasswordsEntry:
      return true;
    default:
      return false;
  }
}

void PasswordManualFallbackFlow::OnFetchCompleted() {
  if (flow_state_ == FlowState::kCreated) {
    flow_state_ = FlowState::kSuggestedPasswordsReady;
  } else if (flow_state_ == FlowState::kAllUserPasswordsFetched) {
    flow_state_ = FlowState::kFlowInitialized;
    // The flow state transition to `FlowState::kFlowInitialized` can happen
    // only once.
    metrics_recorder_.RecordDataFetchingLatency();
    if (on_all_password_data_ready_) {
      std::move(on_all_password_data_ready_).Run();
    }
  }
}

void PasswordManualFallbackFlow::OnSavedPasswordsChanged(
    const PasswordStoreChangeList& changes) {
  if (flow_state_ == FlowState::kCreated) {
    flow_state_ = FlowState::kAllUserPasswordsFetched;
  } else if (flow_state_ == FlowState::kSuggestedPasswordsReady) {
    flow_state_ = FlowState::kFlowInitialized;
    // The flow state transition to `FlowState::kFlowInitialized` can happen
    // only once.
    metrics_recorder_.RecordDataFetchingLatency();
    if (on_all_password_data_ready_) {
      std::move(on_all_password_data_ready_).Run();
    }
  }
}

void PasswordManualFallbackFlow::RunFlow(
    autofill::FieldRendererId field_id,
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  field_id_ = field_id;
  bounds_ = bounds;
  text_direction_ = text_direction;

  if (flow_state_ != FlowState::kFlowInitialized) {
    on_all_password_data_ready_ =
        base::BindOnce(&PasswordManualFallbackFlow::RunFlowImpl,
                       base::Unretained(this), bounds, text_direction);
    return;
  }
  RunFlowImpl(bounds, text_direction);
}

absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordManualFallbackFlow::GetDriver() {
  return password_manager_driver_.get();
}

void PasswordManualFallbackFlow::OnSuggestionsShown() {}

void PasswordManualFallbackFlow::OnSuggestionsHidden() {}

void PasswordManualFallbackFlow::DidSelectSuggestion(
    const Suggestion& suggestion) {
  CHECK(SupportsSuggestionType(suggestion.type));
  if (!suggestion.is_acceptable) {
    return;
  }
  switch (suggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
      password_manager_driver_->PreviewSuggestion(
          GetUsernameFromLabel(suggestion.labels[0][0].value),
          suggestion.GetPayload<Suggestion::PasswordSuggestionDetails>()
              .password);
      break;
    case autofill::SuggestionType::kPasswordFieldByFieldFilling:
      password_manager_driver_->PreviewField(field_id_,
                                             suggestion.main_text.value);
      break;
    case autofill::SuggestionType::kFillPassword:
    case autofill::SuggestionType::kViewPasswordDetails:
    case autofill::SuggestionType::kAllSavedPasswordsEntry:
      // No preview for these suggestions.
      break;
    default:
      // Other suggestion types are not supported.
      NOTREACHED_NORETURN();
  }
}

void PasswordManualFallbackFlow::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  CHECK(SupportsSuggestionType(suggestion.type));
  if (!suggestion.is_acceptable) {
    return;
  }
  switch (suggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
      MaybeAuthenticateBeforeFilling(base::BindOnce(
          &PasswordManagerDriver::FillSuggestion,
          base::Unretained(password_manager_driver_),
          GetUsernameFromLabel(suggestion.labels[0][0].value),
          suggestion.GetPayload<Suggestion::PasswordSuggestionDetails>()
              .password));
      break;
    case autofill::SuggestionType::kPasswordFieldByFieldFilling:
      password_manager_driver_->FillField(field_id_,
                                          suggestion.main_text.value);
      break;
    case autofill::SuggestionType::kFillPassword: {
      Suggestion::PasswordSuggestionDetails payload =
          suggestion.GetPayload<Suggestion::PasswordSuggestionDetails>();
      auto filling_callback = base::BindOnce(
          &PasswordManualFallbackFlow::MaybeAuthenticateBeforeFilling,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&PasswordManagerDriver::FillField,
                         base::Unretained(password_manager_driver_), field_id_,
                         payload.password));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
      if (payload.is_cross_domain) {
        cross_domain_confirmation_popup_controller_ =
            password_client_->ShowCrossDomainConfirmationPopup(
                bounds_, text_direction_,
                password_manager_driver_->GetLastCommittedURL(),
                payload.display_signon_realm, std::move(filling_callback));
        break;
      }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

      std::move(filling_callback).Run();
      break;
    }
    case autofill::SuggestionType::kViewPasswordDetails:
      // TODO(b/324242001): Trigger password details dialog.
      break;
    case autofill::SuggestionType::kAllSavedPasswordsEntry:
      password_client_->NavigateToManagePasswordsPage(
          ManagePasswordsReferrer::kPasswordDropdown);
      metrics_util::LogPasswordDropdownItemSelected(
          metrics_util::PasswordDropdownSelectedOption::kShowAll,
          password_client_->IsOffTheRecord());
      break;
    default:
      // Other suggestion types are not supported.
      NOTREACHED_NORETURN();
  }
  autofill_client_->HideAutofillSuggestions(
      autofill::SuggestionHidingReason::kAcceptSuggestion);
}

void PasswordManualFallbackFlow::DidPerformButtonActionForSuggestion(
    const Suggestion& suggestion) {
  // Button actions do currently not exist for password entries.
  NOTREACHED_NORETURN();
}

bool PasswordManualFallbackFlow::RemoveSuggestion(
    const Suggestion& suggestion) {
  // Password suggestions cannot be deleted this way.
  // See http://crbug.com/329038#c15
  return false;
}

void PasswordManualFallbackFlow::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
}

autofill::FillingProduct PasswordManualFallbackFlow::GetMainFillingProduct()
    const {
  return autofill::FillingProduct::kPassword;
}

void PasswordManualFallbackFlow::RunFlowImpl(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  IsTriggeredOnPasswordForm on_password_form(
      password_form_cache_->HasPasswordForm(password_manager_driver_,
                                            field_id_));
  // TODO(b/331409076): Fetch suggested passwords and pass them to the
  // suggestion generator.
  std::vector<Suggestion> suggestions =
      suggestion_generator_.GetManualFallbackSuggestions(
          form_fetcher_->GetBestMatches(),
          base::make_span(passwords_presenter_->GetSavedPasswords()),
          on_password_form);
  // TODO(crbug.com/41474723): Set the right `form_control_ax_id`.
  autofill::AutofillClient::PopupOpenArgs open_args(
      bounds, text_direction, std::move(suggestions),
      autofill::AutofillSuggestionTriggerSource::kManualFallbackPasswords,
      /*form_control_ax_id=*/0, autofill::PopupAnchorType::kField);
  autofill_client_->ShowAutofillSuggestions(open_args,
                                            weak_ptr_factory_.GetWeakPtr());
}

void PasswordManualFallbackFlow::MaybeAuthenticateBeforeFilling(
    base::OnceClosure fill_fields) {
  // TODO(b/324241248): Conditionally trigger consent dialog and fill
  // password.
  CancelBiometricReauthIfOngoing();
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      password_client_->GetDeviceAuthenticator();
  // Note: this is currently only implemented on Android, Mac and Windows.
  // For other platforms, the `authenticator` will be null.
  if (!password_client_->CanUseBiometricAuthForFilling(authenticator.get())) {
    std::move(fill_fields).Run();
  } else {
    authenticator_ = std::move(authenticator);

    std::u16string message;
    auto on_reath_complete =
        base::BindOnce(&PasswordManualFallbackFlow::OnBiometricReauthCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(fill_fields));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    const std::u16string origin = base::UTF8ToUTF16(GetShownOrigin(
        url::Origin::Create(password_manager_driver_->GetLastCommittedURL())));
    message =
        l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif
    authenticator_->AuthenticateWithMessage(
        message, metrics_util::TimeCallbackMediumTimes(
                     std::move(on_reath_complete),
                     "PasswordManager.PasswordFilling.AuthenticationTime2"));
  }
}

void PasswordManualFallbackFlow::OnBiometricReauthCompleted(
    base::OnceClosure fill_fields,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded) {
    return;
  }
  std::move(fill_fields).Run();
}

void PasswordManualFallbackFlow::CancelBiometricReauthIfOngoing() {
  if (!authenticator_) {
    return;
  }
  authenticator_->Cancel();
  authenticator_.reset();
}

}  // namespace password_manager
