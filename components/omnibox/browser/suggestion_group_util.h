// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_

#include "components/omnibox/browser/autocomplete_input.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace omnibox {

// The toolbelt appears even lower than IPH, but it keeps high relevance to be
// preserved on the list. It's almost like a UI element that comes after all the
// matches, but it works like a match because it still needs to be accessible as
// a popup selection, follow tab order, read a11y text, etc.
inline constexpr int kToolbeltRelevance = 2000;
// Verbatim suggestion is assigned the highest relevance to ensure #1 it appears
// at the top and #2 duplicate suggestions are merged to the verbatim suggestion
// and not the other way around.
inline constexpr int kVerbatimMatchZeroSuggestRelevance = 1602;
// Clipboard suggestion is assigned the 2nd highest relevance to ensure it
// appears after the verbatim suggestion.
inline constexpr int kClipboardMatchZeroSuggestRelevance = 1601;
// MostVisited suggestions on Web are assigned the 3rd highest relevance to
// ensure they appear after the Clipboard suggestion.
inline constexpr int kMostVisitedTilesZeroSuggestHighRelevance = 1600;
// OpenTab suggestions are assigned the 4th highest relevance to ensure they
// appear #1 below the MostVisited tiles and #2 above the remote zero-prefix
// suggestions which have relevance scores between 550-1400.
inline constexpr int kOpenTabMatchZeroSuggestRelevance = 1500;
// Contextual action (pedal) suggestions are assigned a relevance of 1500 to
// ensure they appear above the remote zero-prefix suggestions which have
// relevance scores between 550-1400.
inline constexpr int kContextualActionZeroSuggestRelevance = 1500;
// However, when on SRP, the action needs to be sorted below other matches in
// the DesktopSRPZpsSection so a lower relevance is used to avoid conflict.
inline constexpr int kContextualActionZeroSuggestRelevanceLow = 410;
// Remote zero-prefix suggestions are assigned a default relevance of 1400 when
// not explicitly specified by the server, ensuring consistency with their usual
// relevance scores.
inline constexpr int kDefaultRemoteZeroSuggestRelevance = 1400;
// Local History zero-prefix suggestions are assigned a default relevance of 500
// to ensure #1 they appear below and #2 are merged into (if are duplicates) the
// remote zero-prefix suggestions which have relevance scores between 550-1400.
inline constexpr int kLocalHistoryZeroSuggestRelevance = 500;
// MostVisited suggestions on SRP are assigned a default relevance of 500 to
// ensure they appear below the remote zero-prefix suggestions which have
// relevance scores between 550-1400.
inline constexpr int kMostVisitedTilesZeroSuggestLowRelevance = 500;
// Unscoped Extension suggestions are assigned a default relevance of 400 to
// ensure they appear below all other suggestions, except for IPH suggestions.
inline constexpr int kUnscopedExtensionZeroSuggestRelevance = 400;
// IPH suggestions get a default relevance of 300 to ensure they appear at the
// bottom.
inline constexpr int kIPHZeroSuggestRelevance = 300;

using GroupConfigMap = std::unordered_map<GroupId, GroupConfig>;

// Builds the pre-defined static groups that are useful for sorting suggestions.
const omnibox::GroupConfigMap& BuildDefaultGroupsForInput(
    const AutocompleteInput& input,
    bool is_incognito);

// Returns the omnibox::GroupId enum object corresponding to |value|, or
// omnibox::GROUP_INVALID when there is no corresponding enum object.
GroupId GroupIdForNumber(int value);

// Releases all previously created group definitions for testing purposes.
void ResetDefaultGroupsForTest();

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
