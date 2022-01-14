// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"

namespace autofill {
namespace payments {

PaymentsRequest::~PaymentsRequest() = default;

base::Value PaymentsRequest::BuildRiskDictionary(
    const std::string& encoded_risk_data) {
  base::Value risk_data(base::Value::Type::DICTIONARY);
#if BUILDFLAG(IS_IOS)
  // Browser fingerprinting is not available on iOS. Instead, we generate
  // RiskAdvisoryData.
  risk_data.SetKey("message_type", base::Value("RISK_ADVISORY_DATA"));
  risk_data.SetKey("encoding_type", base::Value("BASE_64_URL"));
#else
  risk_data.SetKey("message_type",
                   base::Value("BROWSER_NATIVE_FINGERPRINTING"));
  risk_data.SetKey("encoding_type", base::Value("BASE_64"));
#endif

  risk_data.SetKey("value", base::Value(encoded_risk_data));

  return risk_data;
}

base::Value PaymentsRequest::BuildCustomerContextDictionary(
    int64_t external_customer_id) {
  base::Value customer_context(base::Value::Type::DICTIONARY);
  customer_context.SetKey(
      "external_customer_id",
      base::Value(base::NumberToString(external_customer_id)));
  return customer_context;
}

}  // namespace payments
}  // namespace autofill
