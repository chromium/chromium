// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"

#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

bool ShouldShowCardMetadata(const CreditCard& card) {
  // The product name and the art image must both be valid.
  return !card.product_description().empty() &&
         card.card_art_url().is_valid();
}

bool DidDisplayBenefitForCard(const CreditCard& card,
                              const AutofillClient& autofill_client) {
  const PaymentsDataManager& pay_dm =
      autofill_client.GetPersonalDataManager().payments_data_manager();
  return pay_dm.IsCardEligibleForBenefits(card) &&
         !pay_dm
              .GetApplicableBenefitDescriptionForCardAndOrigin(
                  card,
                  autofill_client.GetLastCommittedPrimaryMainFrameOrigin(),
                  autofill_client.GetAutofillOptimizationGuideDecider())
              .empty();
}

bool IsVcn3dsEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableVcn3dsAuthentication) &&
         !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);
}

}  // namespace autofill
