// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"

#include "components/autofill/core/browser/data_model/credit_card.h"
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

}  // namespace autofill
