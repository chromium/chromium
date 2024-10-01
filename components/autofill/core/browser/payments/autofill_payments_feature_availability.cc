// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

bool ShouldShowCardMetadata(const CreditCard& card) {
  // The product name and the art image must both be valid.
  return !card.product_description().empty() &&
         card.card_art_url().is_valid() &&
         base::FeatureList::IsEnabled(
             features::kAutofillEnableCardProductName) &&
         base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage);
}

bool DidDisplayBenefitForCard(
    const CreditCard& card,
    const AutofillClient& autofill_client,
    const PaymentsDataManager& payments_data_manager) {
  return payments_data_manager.IsCardEligibleForBenefits(card) &&
         !payments_data_manager
              .GetApplicableBenefitDescriptionForCardAndOrigin(
                  card,
                  autofill_client.GetLastCommittedPrimaryMainFrameOrigin(),
                  autofill_client.GetAutofillOptimizationGuide())
              .empty();
}

bool IsVcn3dsEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableVcn3dsAuthentication) &&
         !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);
}

bool IsSaveCardLoadingAndConfirmationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSaveCardLoadingAndConfirmation);
}

}  // namespace autofill
