// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_logging_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"

namespace {

// This function provides a logging implementation that aligns with the original
// definition of the `DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES()` macro, which is
// currently being used to log the `FocusToOpenTimeAnyPopupState3` Omnibox
// metric.
void LogHistogramMediumTimes(const std::string& histogram_name,
                             base::TimeDelta elapsed) {
  base::UmaHistogramCustomTimes(histogram_name, elapsed, base::Milliseconds(10),
                                base::Minutes(3), 50);
}

}  // namespace

namespace omnibox {

void LogFocusToOpenTime(
    base::TimeDelta elapsed,
    bool is_zero_prefix,
    ::metrics::OmniboxEventProto::PageClassification page_classification,
    const AutocompleteMatch& match,
    size_t action_index) {
  LogHistogramMediumTimes("Omnibox.FocusToOpenTimeAnyPopupState3", elapsed);

  std::string summarized_result_type;
  switch (OmniboxMetricsProvider::GetClientSummarizedResultType(
      match.GetOmniboxEventResultType(action_index))) {
    case ClientSummarizedResultType::kSearch:
      summarized_result_type = "SEARCH";
      break;
    case ClientSummarizedResultType::kUrl:
      summarized_result_type = "URL";
      break;
    default:
      summarized_result_type = "OTHER";
      break;
  }

  LogHistogramMediumTimes(
      base::StrCat(
          {"Omnibox.FocusToOpenTimeAnyPopupState3.BySummarizedResultType.",
           summarized_result_type}),
      elapsed);

  const std::string page_context =
      ::metrics::OmniboxEventProto::PageClassification_Name(
          page_classification);
  LogHistogramMediumTimes(
      base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ByPageContext.",
                    page_context}),
      elapsed);

  LogHistogramMediumTimes(
      base::StrCat(
          {"Omnibox.FocusToOpenTimeAnyPopupState3.BySummarizedResultType.",
           summarized_result_type, ".ByPageContext.", page_context}),
      elapsed);

  if (is_zero_prefix) {
    LogHistogramMediumTimes("Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest",
                            elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat(
            {"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest.ByPageContext.",
             page_context}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type, ".ByPageContext.", page_context}),
        elapsed);
  } else {
    LogHistogramMediumTimes(
        "Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest", elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "ByPageContext.",
                      page_context}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type, ".ByPageContext.", page_context}),
        elapsed);
  }
}

}  // namespace omnibox
