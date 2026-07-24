// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_PARAMS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_PARAMS_H_

#include <string>

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

namespace payments::facilitated {

struct AccountLinkingParams {
  explicit AccountLinkingParams(FacilitatedPaymentsType fop_type)
      : fop_type(fop_type) {}
  AccountLinkingParams(const AccountLinkingParams&) = default;
  AccountLinkingParams& operator=(const AccountLinkingParams&) = default;

  // The type of form-of-payment being linked (e.g. Ewallet, Pix).
  // Determines which UI strings and metrics to use.
  FacilitatedPaymentsType fop_type;

  // The display name of the form of payment (e.g., eWallet app name).
  // Shown directly in the prompt UI context.
  std::u16string fop_display_name;

  // The current number of times the prompt has been shown without acceptance.
  // Determines whether the prompt is blocked or proceeds.
  int strike_count = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_PARAMS_H_
