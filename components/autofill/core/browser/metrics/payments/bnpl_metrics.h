// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_

#include <string_view>

namespace autofill::autofill_metrics {

// Logs if the buy-now-pay-later preference is changed by the user through the
// pay-over-time toggle in the payment methods settings page. Records true when
// the user switches on buy-now-pay-later. Records false when the user switches
// off buy-now-pay-later.
void LogBnplPrefToggled(bool enabled);

// Logs the number of BNPL issuers synced at startup.
void LogBnplIssuersSyncedCountAtStartup(int count);

// Logs that the BNPL ToS dialog was shown.
void LogBnplTosDialogShown(std::string_view issuer_id);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
