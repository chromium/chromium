// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill {
namespace payments {

int64_t GetBillingCustomerId(PersonalDataManager* personal_data_manager,
                             PrefService* pref_service,
                             bool should_log_validity) {
  DCHECK(personal_data_manager);
  DCHECK(pref_service);

  if (base::FeatureList::IsEnabled(
          features::kAutofillUsePaymentsCustomerData) ||
      base::FeatureList::IsEnabled(
          features::kAutofillEnableAccountWalletStorage)) {
    // Get billing customer ID from the synced PaymentsCustomerData.
    PaymentsCustomerData* customer_data =
        personal_data_manager->GetPaymentsCustomerData();
    if (customer_data && !customer_data->customer_id.empty()) {
      int64_t billing_customer_id = 0;
      if (base::StringToInt64(base::StringPiece(customer_data->customer_id),
                              &billing_customer_id)) {
        if (should_log_validity) {
          AutofillMetrics::LogPaymentsCustomerDataBillingIdStatus(
              AutofillMetrics::BillingIdStatus::VALID);
        }
        return billing_customer_id;
      } else {
        if (should_log_validity) {
          AutofillMetrics::LogPaymentsCustomerDataBillingIdStatus(
              AutofillMetrics::BillingIdStatus::PARSE_ERROR);
        }
      }
    } else {
      if (should_log_validity) {
        AutofillMetrics::LogPaymentsCustomerDataBillingIdStatus(
            AutofillMetrics::BillingIdStatus::MISSING);
      }
    }
  }

  // Get billing customer ID from priority preferences.
  return static_cast<int64_t>(
      pref_service->GetDouble(prefs::kAutofillBillingCustomerNumber));
}

}  // namespace payments
}  // namespace autofill
