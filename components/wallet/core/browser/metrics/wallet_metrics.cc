// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/metrics/wallet_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/wallet/core/browser/data_models/data_model_utils.h"

namespace wallet::metrics {

void LogOptInEvent(PassCategory pass_category,
                   WalletablePassOptInFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.OptIn.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

void LogServerExtractionEvent(
    PassCategory pass_category,
    WalletablePassServerExtractionFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.ServerExtraction.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

void LogSaveEvent(PassCategory pass_category,
                  WalletablePassSaveFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.Save.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

}  // namespace wallet::metrics
