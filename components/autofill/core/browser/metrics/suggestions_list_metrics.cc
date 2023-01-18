// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill::autofill_metrics {

void LogAutofillSuggestionAcceptedIndex(int index,
                                        PopupType popup_type,
                                        bool off_the_record) {
  const int uma_index = std::min(index, kMaxBucketsCount);
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex", uma_index);

  switch (popup_type) {
    case PopupType::kCreditCards:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.CreditCard",
                               uma_index);
      break;
    case PopupType::kAddresses:
    case PopupType::kPersonalInformation:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Profile",
                               uma_index);
      break;
    case PopupType::kPasswords:
    case PopupType::kUnspecified:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Other",
                               uma_index);
      break;
  }

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));

  base::UmaHistogramBoolean("Autofill.SuggestionAccepted.OffTheRecord",
                            off_the_record);
}

}  // namespace autofill::autofill_metrics
