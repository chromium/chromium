// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
namespace payments {

namespace {
constexpr int kCustomerHasNoBillingCustomerNumber = 0;
}

int64_t GetBillingCustomerId(PersonalDataManager* personal_data_manager) {
  DCHECK(personal_data_manager);

  // Get billing customer ID from the synced PaymentsCustomerData.
  PaymentsCustomerData* customer_data =
      personal_data_manager->GetPaymentsCustomerData();
  if (customer_data && !customer_data->customer_id.empty()) {
    int64_t billing_customer_id = 0;
    if (base::StringToInt64(base::StringPiece(customer_data->customer_id),
                            &billing_customer_id)) {
      return billing_customer_id;
    }
  }
  return kCustomerHasNoBillingCustomerNumber;
}

bool HasGooglePaymentsAccount(PersonalDataManager* personal_data_manager) {
  return GetBillingCustomerId(personal_data_manager) !=
         kCustomerHasNoBillingCustomerNumber;
}

bool IsCreditCardNumberSupported(
    const std::u16string& card_number,
    const std::vector<std::pair<int, int>>& supported_card_bin_ranges) {
  std::u16string stripped_number = CreditCard::StripSeparators(card_number);
  return base::ranges::any_of(supported_card_bin_ranges, [&](const auto& p) {
    auto& [bin_low, bin_high] = p;
    unsigned long range_num_of_digits = base::NumberToString(bin_low).size();
    DCHECK_EQ(range_num_of_digits, base::NumberToString(bin_high).size());
    // The first n digits of credit card number, where n is the number of
    // digits in range's starting/ending number.
    int first_digits_start, first_digits_end;
    base::StringToInt(stripped_number.substr(0, range_num_of_digits),
                      &first_digits_start);
    base::StringToInt(stripped_number.substr(0, range_num_of_digits),
                      &first_digits_end);
    return first_digits_start >= bin_low && first_digits_end <= bin_high;
  });
}

}  // namespace payments
}  // namespace autofill
