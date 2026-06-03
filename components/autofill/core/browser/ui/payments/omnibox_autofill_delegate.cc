// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include <memory>
#include <set>

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {

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
          autofill::FormStructure::CreditCardFormCompleteness::
              kCompleteCreditCardForm)) {
    LogOmniboxAutofillShowChipDecisionPart1(
        OmniboxAutofillShowChipDecisionPart1::kNotCompleteCreditCardForm);
    return;
  }

  // The client context and credit card form must be secure (not HTTP).
  if (IsFormOrClientNonSecure(*client_, *form_structure)) {
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

  // More checks to follow as implementation continues...

  LogOmniboxAutofillShowChipDecisionPart1(
      OmniboxAutofillShowChipDecisionPart1::kSuccess);
}

void OmniboxAutofillDelegate::OnAutofillManagerStateChanged(
    autofill::AutofillManager& manager,
    autofill::AutofillManager::LifecycleState previous,
    autofill::AutofillManager::LifecycleState current) {
  switch (previous) {
    case autofill::AutofillManager::LifecycleState::kActive:
      client_->GetPaymentsAutofillClient()->HideOmniboxAutofillChip();
      break;
    default:
      break;
  }
}

void OmniboxAutofillDelegate::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (const FormGlobalId& id : removed_forms) {
    if (id == trigger_form_global_id_) {
      client_->GetPaymentsAutofillClient()->HideOmniboxAutofillChip();
      return;
    }
  }
}

void OmniboxAutofillDelegate::OnGetIntersectionObserverInfo(bool is_visible) {
  if (!is_visible) {
    return;
  }
  client_->GetPaymentsAutofillClient()->ShowOmniboxAutofillChip();
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

}  // namespace autofill
