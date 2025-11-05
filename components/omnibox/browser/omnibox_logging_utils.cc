// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_logging_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "url/gurl.h"

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

// Find the number of IPv4 parts if the user inputs a URL with an IP address
// host. Returns 0 if the user does not manually types the full IP address.
size_t CountNumberOfIPv4Parts(const std::u16string& text,
                              const GURL& url,
                              size_t completed_length) {
  if (!url.HostIsIPAddress() || !url.SchemeIsHTTPOrHTTPS() ||
      completed_length > 0) {
    return 0;
  }

  url::Parsed parsed = url::ParseStandardUrl(text);
  if (!parsed.host.is_valid()) {
    return 0;
  }

  size_t parts = 1;
  bool potential_part = false;
  for (int i = parsed.host.begin; i < parsed.host.end(); i++) {
    if (text[i] == '.') {
      potential_part = true;
    }
    if (potential_part && text[i] >= '0' && text[i] <= '9') {
      parts++;
      potential_part = false;
    }
  }
  return parts;
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

void RecordActionShownForAllActions(const AutocompleteResult& result,
                                    OmniboxPopupSelection executed_selection) {
  // Record the presence of all actions in the result set.
  for (size_t line_index = 0; line_index < result.size(); ++line_index) {
    const AutocompleteMatch& match = result.match_at(line_index);
    // Record the presence of the takeover action on this line, if any.
    if (match.takeover_action) {
      match.takeover_action->RecordActionShown(
          line_index,
          /*executed=*/line_index == executed_selection.line &&
              executed_selection.state == OmniboxPopupSelection::NORMAL);
    }
    for (size_t action_index = 0; action_index < match.actions.size();
         ++action_index) {
      match.actions[action_index]->RecordActionShown(
          line_index, /*executed=*/line_index == executed_selection.line &&
                          action_index == executed_selection.action_index &&
                          executed_selection.state ==
                              OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
    }
  }
}

void LogIPv4PartsCount(const std::u16string& user_text,
                       const GURL& destination_url,
                       size_t completed_length) {
  size_t ipv4_parts_count =
      CountNumberOfIPv4Parts(user_text, destination_url, completed_length);
  // The histogram is collected to decide if shortened IPv4 addresses
  // like 127.1 should be deprecated.
  // Only valid IP addresses manually inputted by the user will be counted.
  if (ipv4_parts_count > 0) {
    base::UmaHistogramCounts100("Omnibox.IPv4AddressPartsCount",
                                ipv4_parts_count);
  }
}

}  // namespace omnibox
