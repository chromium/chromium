// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_

namespace autofill {

class AutofillClient;
class CreditCard;
class PaymentsDataManager;

// Returns whether the `card` is shown in an Autofill suggestion dropdown with a
// benefit label.
bool DidDisplayBenefitForCard(const CreditCard& card,
                              const AutofillClient& autofill_client,
                              const PaymentsDataManager& payments_data_manager);

// Returns whether the `card` is populated with a card art image and a card
// product name and whether they both should be shown.
bool ShouldShowCardMetadata(const CreditCard& card);

// Returns whether VCN 3DS authentication is enabled and can be used as an
// authentication option.
bool IsVcn3dsEnabled();

// Returns whether the save card dialog will present a loading spinner when
// uploading the card to the server and present a confirmation dialog with the
// result when completed.
bool IsSaveCardLoadingAndConfirmationEnabled();

// TODO(crbug.com/40263500): Move here payments related feature availability
// checks from autofill_experiments.

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_
