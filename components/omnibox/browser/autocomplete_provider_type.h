// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_TYPE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_TYPE_H_

#include <ostream>
#include <type_traits>

#include "third_party/metrics_proto/omnibox_event.pb.h"

// Different AutocompleteProvider implementations. Ordered consistent with
// `OmniboxEventProto`.
enum class AutocompleteProviderType : int {
  kNone = 0,
  kHistoryUrl = 1 << 0,
  kHistoryQuick = 1 << 1,
  kSearch = 1 << 2,
  kVoiceSuggest = 1 << 3,
  kCalculator = 1 << 4,
  kKeyword = 1 << 5,
  kBuiltin = 1 << 6,
  kShortcuts = 1 << 7,
  kBookmark = 1 << 8,
  kZeroSuggest = 1 << 9,
  kMostVisitedSites = 1 << 10,
  kVerbatimMatch = 1 << 11,
  kClipboard = 1 << 12,
  kDocument = 1 << 13,
  kOnDeviceHead = 1 << 14,
  kZeroSuggestLocalHistory = 1 << 15,
  kQueryTile = 1 << 16,
  kHistoryCluster = 1 << 17,
  kHistoryFuzzy = 1 << 18,
  kOpenTab = 1 << 19,
};

AutocompleteProviderType operator&(const AutocompleteProviderType& lhs,
                                   const AutocompleteProviderType& rhs);

AutocompleteProviderType operator|(const AutocompleteProviderType& lhs,
                                   const AutocompleteProviderType& rhs);

AutocompleteProviderType operator&=(AutocompleteProviderType& lhs,
                                    const AutocompleteProviderType& rhs);

AutocompleteProviderType operator|=(AutocompleteProviderType& lhs,
                                    const AutocompleteProviderType& rhs);

AutocompleteProviderType operator~(const AutocompleteProviderType& type);

bool operator!(const AutocompleteProviderType& type);

std::ostream& operator<<(std::ostream& out,
                         const AutocompleteProviderType& type);

const char* AutocompleteProviderTypeToString(AutocompleteProviderType type);

metrics::OmniboxEventProto_ProviderType
AutocompleteProviderTypeToOmniboxEventProviderType(
    AutocompleteProviderType type);

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_TYPE_H_
