// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/payments/constants.h"

namespace autofill::autofill_metrics {
namespace {
std::string GetHistogramSuffixFromIssuerId(std::string_view issuer_id) {
  if (issuer_id == kBnplAffirmIssuerId) {
    return "Affirm";
  } else if (issuer_id == kBnplZipIssuerId) {
    return "Zip";
  } else if (issuer_id == kBnplAfterpayIssuerId) {
    return "Afterpay";
  }
  NOTREACHED();
}
}  // namespace

void LogBnplPrefToggled(bool enabled) {
  base::UmaHistogramBoolean("Autofill.SettingsPage.BnplToggled", enabled);
}

void LogBnplIssuersSyncedCountAtStartup(int count) {
  base::UmaHistogramCounts100("Autofill.Bnpl.IssuersSyncedCount.Startup",
                              count);
}

void LogBnplTosDialogShown(std::string_view issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.TosDialogShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, /*sample=*/true);
}

}  // namespace autofill::autofill_metrics
