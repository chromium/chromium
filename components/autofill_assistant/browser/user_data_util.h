// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_

#include <vector>
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile);

// Sorts the given autofill profiles based on completeness, and returns a
// vector of profile indices in sorted order. Full profiles will be ordered
// before empty ones, and for equally complete profiles, this falls back to
// sorting based on the profile names.
std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Get the default selection for the current list of profiles. Returns -1 if no
// default selection is possible.
int GetDefaultContactProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Sorts the given autofill profiles based on completeness, and returns a
// vector of profile indices in sorted order. Full profiles will be ordered
// before empty ones, and for equally complete profiles, this falls back to
// sorting based on the profile names.
std::vector<int> SortAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Get the default selection for the current list of profiles. Returns -1 if no
// default selection is possible.
int GetDefaultAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Sorts the given payment instruments by completeness, and returns a vector
// of payment instrument indices in sorted order. Full payment instruments will
// be ordered before empty ones, and for equally complete payment instruments,
// this falls back to sorting based on the full name on the credit card.
std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

// Get the default selection for the current list of payment instruments.
// Returns -1 if no default selection is possible.
int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

// Compare contact fields only. This comparison checks a subset of
// AutofillProfile::Compare. Falls back to comparing the GUIDs if nothing else
// is to be compared.
bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b);

bool IsCompleteContact(const autofill::AutofillProfile* profile,
                       const CollectUserDataOptions& collect_user_data_options);

bool IsCompleteShippingAddress(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options);

bool IsCompleteCreditCard(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_profile,
    const CollectUserDataOptions& collect_user_data_options);

ClientStatus GetFormattedAutofillValue(const AutofillValue& autofill_value,
                                       const UserData* user_data,
                                       std::string* out_value);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
