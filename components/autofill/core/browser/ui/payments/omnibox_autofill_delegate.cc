// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include <algorithm>
#include <memory>
#include <set>

#include "base/check_deref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {

namespace {

bool IsValidOmniboxAutofillSuggestion(SuggestionType type) {
  switch (type) {
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return false;
  }
}

}  // namespace

using autofill_metrics::OmniboxAutofillShowChipDecisionPart1;

OmniboxAutofillDelegate::OmniboxAutofillDelegate(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {
  autofill_managers_observation_.Observe(client);
}

OmniboxAutofillDelegate::~OmniboxAutofillDelegate() = default;

void OmniboxAutofillDelegate::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  if (candidate_form_found_) {
    // Candidate already found and awaiting user action asynchronously.
    return;
  }

  // Only run checks using the outermost AutofillManager to avoid having
  // multiple managers triggering the logic flow at once.
  if (!IsOutermostMainFrameActiveAutofillManager(manager)) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kNotActiveOutermostMainFrameBam);
    return;
  }

  // Respect the kAutofillCreditCardEnabled pref, which can be toggled by
  // users, enterprise admins, or extensions.
  if (!client_->GetPaymentsAutofillClient()
           ->GetPaymentsDataManager()
           .IsAutofillPaymentMethodsEnabled()) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::
            kAutofillPaymentMethodsPolicyDisabled);
    return;
  }

  // The user must have credit cards saved in order to have something to
  // autofill.
  if (client_->GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetCreditCards()
          .empty()) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kNoCreditCardsSaved);
    return;
  }

  // The parsed form must have credit card number and expiration date fields.
  const FormStructure* form_structure = manager.FindCachedFormById(form_id);
  if (!form_structure) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kCouldNotFindCachedForm);
    return;
  }
  if (!form_structure->IsCompleteCreditCardForm(
          FormStructure::CreditCardFormCompleteness::kCompleteCreditCardForm)) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kNotCompleteCreditCardForm);
    return;
  }

  // The client context and credit card form must be secure (not HTTP).
  if (!client_->IsContextSecure()) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kFormOrClientContextNotSecure);
    return;
  }

  // Iterate over all AutofillFields in the FormStructure, paying attention to
  // the frame they are in (main vs. iframe) as well as ensuring there's only a
  // single CREDIT_CARD_NUMBER type.
  bool found_credit_card_number_field = false;
  std::set<url::Origin> iframe_origins;
  for (const std::unique_ptr<AutofillField>& field : form_structure->fields()) {
    if (field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
      if (found_credit_card_number_field) {
        LogOmniboxAutofillShowChipDecisionPart1(
            OmniboxAutofillShowChipDecisionPart1::
                kFoundMultipleCreditCardNumberFields);
        return;
      }
      found_credit_card_number_field = true;
    }
    if (!FieldIsInMainFrame(manager, *field)) {
      iframe_origins.insert(field->origin());
    }
  }

  // All fields of the form must be either in the main frame or an allowlisted
  // iframe.
  if (!iframe_origins.empty() &&
      !manager.client().GetAutofillOptimizationGuideDecider()) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kMissingOptimizationGuideDecider);
    return;
  }
  for (const url::Origin& origin : iframe_origins) {
    if (!manager.client()
             .GetAutofillOptimizationGuideDecider()
             ->IsUrlEligibleForOmniboxAutofill(origin.GetURL())) {
      LogOmniboxAutofillShowChipDecisionPart1(
          OmniboxAutofillShowChipDecisionPart1::kNonAllowlistedIframe);
      return;
    }
  }

  // All checks passed! Log the triggering form and field, start the
  // IntersectionObserver, and prevent this logic from running again.
  LogOmniboxAutofillShowChipDecisionPart1(
      OmniboxAutofillShowChipDecisionPart1::kSuccess);
  trigger_form_global_id_ = form_structure->global_id();
  for (const std::unique_ptr<AutofillField>& field : form_structure->fields()) {
    if (field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
      trigger_field_global_id_ = field->global_id();
      break;
    }
  }
  candidate_form_found_ = true;

  // TODO: crbug.com/490214534 - Initiate GetIntersectionObserverInfo(~).
}

void OmniboxAutofillDelegate::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState previous,
    AutofillManager::LifecycleState current) {
  switch (previous) {
    case AutofillManager::LifecycleState::kActive:
      HideOmniboxAutofillChip();
      break;
    default:
      break;
  }
}

void OmniboxAutofillDelegate::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  if (!candidate_form_found_) {
    // Candidate form has not yet been found, so the chip is not being shown.
    return;
  }
  for (const FormGlobalId& id : removed_forms) {
    if (id == trigger_form_global_id_) {
      HideOmniboxAutofillChip();
      return;
    }
  }
}

bool OmniboxAutofillDelegate::OnFilterChanged(const std::u16string& filter) {
  return false;
}

bool OmniboxAutofillDelegate::OnSearchSubmitted(const std::u16string& filter) {
  return false;
}

bool OmniboxAutofillDelegate::IsSearching() const {
  return false;
}

std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
OmniboxAutofillDelegate::GetDriver_DoNotUse() {
  auto* manager = client_->GetAutofillManagerForPrimaryMainFrame();
  if (manager) {
    return &manager->driver();
  }
  return static_cast<AutofillDriver*>(nullptr);
}

void OmniboxAutofillDelegate::OnSuggestionsShown(
    base::span<const Suggestion> suggestions,
    base::optional_ref<const SuggestionMetadata> parent_suggestion_metadata) {
  auto* manager = static_cast<BrowserAutofillManager*>(
      client_->GetAutofillManagerForPrimaryMainFrame());
  if (!manager) {
    return;
  }

  const FormStructure* form =
      manager->FindCachedFormById(trigger_form_global_id_);
  if (!form) {
    return;
  }

  const AutofillField* trigger_field =
      form->GetFieldById(trigger_field_global_id_);
  if (!trigger_field) {
    return;
  }

  // Record local and server card counts to ensure form events log under the
  // correct data suffix (e.g., ".WithOnlyServerData" vs. ".WithNoData").
  manager->GetCreditCardAccessManager()->UpdateCreditCardFormEventLogger();

  // Log duration between form parsing and interaction, maintaining consistency
  // with standard Autofill interaction logging.
  AutofillMetrics::LogParsedFormUntilInteractionTiming(
      base::TimeTicks::Now() - form->form_parsed_timestamp());

  // Treat clicking the "Autofill payment" omnibox chip (which forcefully shows
  // the suggestions bubble) the same as a form interaction.
  if (autofill_metrics::FormEventLoggerBase* logger =
          manager->GetEventFormLogger(*trigger_field);
      logger && ShouldBeParsed(*form, /*log_manager=*/nullptr)) {
    if (logger == &manager->GetCreditCardFormEventLogger()) {
      manager->GetCreditCardFormEventLogger().set_signin_state_for_metrics(
          client_->GetPersonalDataManager()
              .payments_data_manager()
              .GetPaymentsSigninStateForMetrics());
    }
    logger->OnDidInteractWithAutofillableForm(*form);
  }

  // TODO(crbug.com/7988776): Use an omnibox-specific trigger source.
  manager->DidShowSuggestions(
      suggestions, parent_suggestion_metadata, trigger_form_global_id_,
      trigger_field_global_id_,
      AutofillExternalDelegate::UpdateSuggestionsCallback());
}

void OmniboxAutofillDelegate::OnSuggestionsHidden(
    SuggestionHidingReason reason) {
  if (auto* manager = static_cast<BrowserAutofillManager*>(
          client_->GetAutofillManagerForPrimaryMainFrame())) {
    manager->OnSuggestionsHidden(reason);
  }
}

void OmniboxAutofillDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  // TODO(crbug.com/490214497): Implement when payment method suggestion list is
  // hovered.
  NOTIMPLEMENTED();
}

void OmniboxAutofillDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  // TODO(crbug.com/490214497): Implement when payment method suggestion list is
  // clicked.
  NOTIMPLEMENTED();
}

bool OmniboxAutofillDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  return false;
}

void OmniboxAutofillDelegate::ClearPreviewedForm() {
  auto* manager = client_->GetAutofillManagerForPrimaryMainFrame();
  if (manager) {
    manager->driver().RendererShouldClearPreviewedForm();
  }
}

FillingProduct OmniboxAutofillDelegate::GetMainFillingProduct() const {
  return FillingProduct::kCreditCard;
}

void OmniboxAutofillDelegate::OnTabSelected(TabbedPaneTabType tab_type) {
  // Tabbed panes do not exist for Omnibox Autofill.
  NOTREACHED();
}

void OmniboxAutofillDelegate::OnGetIntersectionObserverInfo(bool is_visible) {
  if (!is_visible) {
    return;
  }

  auto* manager = static_cast<BrowserAutofillManager*>(
      client_->GetAutofillManagerForPrimaryMainFrame());
  if (!manager) {
    return;
  }

  const FormStructure* form =
      manager->FindCachedFormById(trigger_form_global_id_);
  if (!form) {
    return;
  }

  const AutofillField* trigger_field =
      form->GetFieldById(trigger_field_global_id_);
  if (!trigger_field) {
    return;
  }

  // TODO(crbug.com/523396583): This generates the full list of suggestions and
  // then filters some out. Not all suggestions returned will be displayed on
  // the Omnibox Autofill bubble, which is why we should not generate them in
  // the first place.
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form->ToFormData(), *form, *trigger_field, *trigger_field, *client_,
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      manager->GetCreditCardFormEventLogger(),
      client_->GetPersonalDataManager()
          .payments_data_manager()
          .GetPaymentsSigninStateForMetrics(),
      /*exclude_virtual_cards=*/false);

  std::erase_if(suggestions, [](const Suggestion& suggestion) {
    return !IsValidOmniboxAutofillSuggestion(suggestion.type);
  });

  // Log the number of credit card suggestions generated, maintaining
  // consistency with standard Autofill suggestion generation logging.
  autofill_metrics::LogSuggestionsCount(suggestions.size(),
                                        FillingProduct::kCreditCard);

  // Log security status of the credit card form when suggestions are generated,
  // similar to standard Autofill suggestions generation.
  AutofillMetrics::LogIsQueriedCreditCardFormSecure(client_->IsContextSecure());

  // Shows the "Autofill payment" chip and initializes the bubble.
  client_->GetPaymentsAutofillClient()->ShowOmniboxAutofillChip(
      std::move(suggestions),
      base::BindRepeating(
          [](base::WeakPtr<OmniboxAutofillDelegate> delegate,
             base::span<const Suggestion> suggestions) {
            if (delegate) {
              delegate->OnSuggestionsShown(suggestions, std::nullopt);
            }
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<OmniboxAutofillDelegate> delegate,
             SuggestionHidingReason reason) {
            if (delegate) {
              delegate->OnSuggestionsHidden(reason);
            }
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&OmniboxAutofillDelegate::DidSelectSuggestion,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&OmniboxAutofillDelegate::DidAcceptSuggestion,
                          weak_ptr_factory_.GetWeakPtr()));
}

bool OmniboxAutofillDelegate::IsOutermostMainFrameActiveAutofillManager(
    AutofillManager& manager) {
  return manager.driver().GetParent() == nullptr &&
         !manager.driver().IsEmbedded() && manager.driver().IsActive();
}

bool OmniboxAutofillDelegate::FieldIsInMainFrame(
    AutofillManager& manager,
    const AutofillField& field) const {
  return field.host_frame() == manager.driver().GetFrameToken() &&
         !manager.driver().GetParent();
}

void OmniboxAutofillDelegate::Reset() {
  candidate_form_found_ = false;
  trigger_form_global_id_ = FormGlobalId();
  trigger_field_global_id_ = FieldGlobalId();
}

void OmniboxAutofillDelegate::HideOmniboxAutofillChip() {
  client_->GetPaymentsAutofillClient()->HideOmniboxAutofillChip();
  Reset();
}

}  // namespace autofill
