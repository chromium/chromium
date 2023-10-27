// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider_type.h"

#include <ostream>
#include <type_traits>

#include "base/notreached.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

AutocompleteProviderType operator&(const AutocompleteProviderType& lhs,
                                   const AutocompleteProviderType& rhs) {
  return static_cast<AutocompleteProviderType>(
      std::underlying_type<AutocompleteProviderType>::type(lhs) &
      std::underlying_type<AutocompleteProviderType>::type(rhs));
}
AutocompleteProviderType operator|(const AutocompleteProviderType& lhs,
                                   const AutocompleteProviderType& rhs) {
  return static_cast<AutocompleteProviderType>(
      std::underlying_type<AutocompleteProviderType>::type(lhs) |
      std::underlying_type<AutocompleteProviderType>::type(rhs));
}
AutocompleteProviderType operator&=(AutocompleteProviderType& lhs,
                                    const AutocompleteProviderType& rhs) {
  return lhs = static_cast<AutocompleteProviderType>(
             std::underlying_type<AutocompleteProviderType>::type(lhs) &
             std::underlying_type<AutocompleteProviderType>::type(rhs));
}
AutocompleteProviderType operator|=(AutocompleteProviderType& lhs,
                                    const AutocompleteProviderType& rhs) {
  return lhs = static_cast<AutocompleteProviderType>(
             std::underlying_type<AutocompleteProviderType>::type(lhs) |
             std::underlying_type<AutocompleteProviderType>::type(rhs));
}
AutocompleteProviderType operator~(const AutocompleteProviderType& type) {
  return static_cast<AutocompleteProviderType>(
      ~std::underlying_type<AutocompleteProviderType>::type(type));
}
bool operator!(const AutocompleteProviderType& type) {
  return !std::underlying_type<AutocompleteProviderType>::type(type);
}
std::ostream& operator<<(std::ostream& out,
                         const AutocompleteProviderType& type) {
  out << std::underlying_type<AutocompleteProviderType>::type(type);
  return out;
}

const char* AutocompleteProviderTypeToString(AutocompleteProviderType type) {
  // When creating a new provider, add the provider type to this function and
  // make sure to also add the appropriate OmniboxProvider variant to the
  // Omnibox.ProviderTime2 histogram (defined in omnibox/histograms.xml) so that
  // the run-time metrics associated with the relevant provider can be properly
  // analyzed. Ordered consistent with `OmniboxEventProto`.
  switch (type) {
    case AutocompleteProviderType::kHistoryUrl:
      return "HistoryURL";
    case AutocompleteProviderType::kHistoryQuick:
      return "HistoryQuick";
    case AutocompleteProviderType::kSearch:
      return "Search";
    case AutocompleteProviderType::kVoiceSuggest:
      return "VoiceSuggest";
    case AutocompleteProviderType::kCalculator:
      return "Calculator";
    case AutocompleteProviderType::kKeyword:
      return "Keyword";
    case AutocompleteProviderType::kBuiltin:
      return "Builtin";
    case AutocompleteProviderType::kShortcuts:
      return "Shortcuts";
    case AutocompleteProviderType::kBookmark:
      return "Bookmark";
    case AutocompleteProviderType::kZeroSuggest:
      return "ZeroSuggest";
    case AutocompleteProviderType::kMostVisitedSites:
      return "MostVisitedSites";
    case AutocompleteProviderType::kVerbatimMatch:
      return "VerbatimMatch";
    case AutocompleteProviderType::kClipboard:
      return "Clipboard";
    case AutocompleteProviderType::kDocument:
      return "Document";
    case AutocompleteProviderType::kOnDeviceHead:
      return "OnDeviceHead";
    case AutocompleteProviderType::kZeroSuggestLocalHistory:
      return "LocalHistoryZeroSuggest";
    case AutocompleteProviderType::kQueryTile:
      return "QueryTile";
    case AutocompleteProviderType::kHistoryCluster:
      return "HistoryCluster";
    case AutocompleteProviderType::kHistoryFuzzy:
      return "HistoryFuzzy";
    case AutocompleteProviderType::kOpenTab:
      return "OpenTab";
    default:
      NOTREACHED() << "Unhandled AutocompleteProviderType " << type;
      return "Unknown";
  }
}

metrics::OmniboxEventProto_ProviderType
AutocompleteProviderTypeToOmniboxEventProviderType(
    AutocompleteProviderType type) {
  // Ordered consistent with `OmniboxEventProto`.
  switch (type) {
    case AutocompleteProviderType::kHistoryUrl:
      return metrics::OmniboxEventProto::HISTORY_URL;
    case AutocompleteProviderType::kHistoryQuick:
      return metrics::OmniboxEventProto::HISTORY_QUICK;
    case AutocompleteProviderType::kSearch:
      return metrics::OmniboxEventProto::SEARCH;
    case AutocompleteProviderType::kVoiceSuggest:
      return metrics::OmniboxEventProto::SEARCH;
    case AutocompleteProviderType::kCalculator:
      // TODO(manukh): Since there's a high likelihood the calc provider won't
      //   launch, log as search provider to avoid the adding then deprecating
      //   the provider in the proto and histograms.
      return metrics::OmniboxEventProto::SEARCH;
    case AutocompleteProviderType::kKeyword:
      return metrics::OmniboxEventProto::KEYWORD;
    case AutocompleteProviderType::kBuiltin:
      return metrics::OmniboxEventProto::BUILTIN;
    case AutocompleteProviderType::kShortcuts:
      return metrics::OmniboxEventProto::SHORTCUTS;
    case AutocompleteProviderType::kBookmark:
      return metrics::OmniboxEventProto::BOOKMARK;
    case AutocompleteProviderType::kZeroSuggest:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case AutocompleteProviderType::kMostVisitedSites:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case AutocompleteProviderType::kVerbatimMatch:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case AutocompleteProviderType::kClipboard:
      return metrics::OmniboxEventProto::CLIPBOARD;
    case AutocompleteProviderType::kDocument:
      return metrics::OmniboxEventProto::DOCUMENT;
    case AutocompleteProviderType::kOnDeviceHead:
      return metrics::OmniboxEventProto::ON_DEVICE_HEAD;
    case AutocompleteProviderType::kZeroSuggestLocalHistory:
      return metrics::OmniboxEventProto::ZERO_SUGGEST_LOCAL_HISTORY;
    case AutocompleteProviderType::kQueryTile:
      return metrics::OmniboxEventProto::QUERY_TILE;
    case AutocompleteProviderType::kHistoryCluster:
      return metrics::OmniboxEventProto::HISTORY_CLUSTER;
    case AutocompleteProviderType::kHistoryFuzzy:
      return metrics::OmniboxEventProto::HISTORY_FUZZY;
    case AutocompleteProviderType::kOpenTab:
      return metrics::OmniboxEventProto::OPEN_TAB;
    default:
      NOTREACHED() << "Unhandled AutocompleteProviderType " << type;
      return metrics::OmniboxEventProto::UNKNOWN_PROVIDER;
  }
}
