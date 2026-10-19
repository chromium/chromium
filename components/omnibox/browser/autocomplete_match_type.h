// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_

#include <string>

struct AutocompleteMatch;

namespace omnibox {

// Type of AutocompleteMatch. Typedef'ed in autocomplete_match.h. Defined here
// to pass the type details back and forth between the browser and renderer.
//
// These values are stored in ShortcutsDatabase and in GetDemotionsByType()
// and cannot be renumbered.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxSuggestionType
//
// Any changes to this enum also requires an update to:
//  - `AutocompleteMatch::GetOmniboxEventResultType()`
//  - `AutocompleteMatch::GetVectorIcon()`
//  - `GetClientSummarizedResultType()`
//  - `AutocompleteMatchTypeToString()`
//  - `GetAccessibilityBaseLabel()`
// clang-format off
enum class AutocompleteMatchType {
  kUrlWhatYouTyped               = 0,   // The input as a URL.
  kHistoryUrl                    = 1,   // A past page whose URL contains the input.
  kHistoryTitle                  = 2,   // A past page whose title contains the input.
  kHistoryBody                   = 3,   // A past page whose body contains the input.
  kHistoryKeyword                = 4,   // A past page whose keyword contains the
                                        // input.
  kNavsuggest                    = 5,   // A suggested URL.
  kSearchWhatYouTyped            = 6,   // The input as a search query (with the
                                        // default engine).
  kSearchHistory                 = 7,   // A past search (with the default engine)
                                        // containing the input.
  kSearchSuggest                 = 8,   // A suggested search (with the default engine)
                                        // query that doesn't fall into one of the more
                                        // specific suggestion categories below.
  kSearchSuggestEntity           = 9,   // A suggested search for an entity.
  kSearchSuggestTail             = 10,  // A suggested search to complete the
                                        // tail of the query.
  kSearchSuggestPersonalized     = 11,  // A personalized suggested search.
  kSearchSuggestProfile          = 12,  // A personalized suggested search for a
                                        // Google+ profile.
  kSearchOtherEngine             = 13,  // A search with a non-default engine.
  kExtensionAppDeprecated        = 14,  // An Extension App with a title/url that
                                        // contains the input (deprecated).
  kContactDeprecated             = 15,  // One of the user's contacts
                                        // (deprecated).
  kBookmarkTitle                 = 16,  // A bookmark whose title contains the
                                        // input.
  kNavsuggestPersonalized        = 17,  // A personalized suggestion URL.
  kCalculator                    = 18,  // A calculator result.
  kClipboardUrl                  = 19,  // A URL based on the clipboard.
  kVoiceSuggest                  = 20,  // An Android-specific type which
                                        // indicates a search from voice
                                        // recognizer.
  kPhysicalWebDeprecated         = 21,  // A Physical Web nearby URL
                                        // (deprecated).
  kPhysicalWebOverflowDeprecated = 22,  // An item representing multiple
                                        // Physical Web nearby URLs
                                        // (deprecated).
  kTabSearchDeprecated           = 23,  // A suggested open tab, based on its
                                        // URL or title, via HQP (deprecated).
  kDocumentSuggestion            = 24,  // A suggested document.
  kPedal                         = 25,  // An omnibox pedal match.
  kClipboardText                 = 26,  // Text based on the clipboard.
  kClipboardImage                = 27,  // An image based on the clipboard.
  kTileSuggestion                = 28,  // A suggestion containing query tiles.
  kTileNavsuggest                = 29,  // A suggestion with navigation tiles.
  kOpenTab                       = 30,  // A URL match amongst the currently open
                                        // tabs.
  kHistoryCluster                = 31,  // A history cluster suggestion.
  kNullResultMessage             = 32,  // A suggestion whose purpose is only to
                                        // deliver a message. This suggestion
                                        // cannot be opened or acted upon.
  kStarterPack                   = 33,  // A URL suggestion that a starter pack
                                        // keyword mode chip attaches to.
  kTileMostVisitedSite           = 34,  // Most Visited Site, shown in a
                                        // Horizontal Render Group.
                                        // Different from kTileNavsuggest which
                                        // is an aggregate type by itself.
  kTileRepeatableQuery           = 35,  // Organic Repeatable Query, shown in a
                                        // Horizontal Render Group.
  kHistoryEmbeddings             = 36,  // A past page whose contents have
                                        // similar embeddings to the query.
  kFeaturedEnterpriseSearch      = 37,  // Site search engines featured by
                                        // Enterprise policy.
  kHistoryEmbeddingsAnswer       = 38,
  kTabGroup                      = 39,  // A tab group match.
  kCrossDeviceTab                = 40,  // A tab opened on another device.
  kRecentlyClosedTab             = 41,  // A tab recently closed on this device.
  kMaxValue                      = kRecentlyClosedTab,
};
// clang-format on

// Converts |type| to a string representation. Used in logging.
std::string AutocompleteMatchTypeToString(AutocompleteMatchType type);

// Use this function to convert integers to AutocompleteMatchType enum values.
// If you're sure it will be valid, you can call CHECK on the return value.
// Returns true if |value| was successfully converted to a valid enum value.
// The valid enum value will be written into |result|.
bool AutocompleteMatchTypeFromInteger(int value, AutocompleteMatchType* result);

// Returns the accessibility label for an AutocompleteMatch |match|
// whose text is |match_text| The accessibility label describes the
// match for use in a screenreader or other assistive technology.
//
// |total_matches|, if non-zero, is used in conjunction with |match_index|
// to append a ", n of m" positional message.
//
// |additional_message_id|, if non-zero, is the message ID for an additional
// message that the base message should be placed into as a parameter -
// this is used for describing a focused or available secondary button.
//
// The |label_prefix_length| is an optional out param that provides the number
// of characters in the label that were added before the actual match_text.
//
// TODO(tommycli): It seems odd that we are passing in both |match| and
// |match_text|. Using just |match.contents| or |match.fill_into_edit| seems
// like it could replace |match_text|. Investigate this.
std::u16string AutocompleteMatchToAccessibilityLabel(
    const AutocompleteMatch& match,
    const std::u16string& header_text,
    const std::u16string& match_text,
    size_t match_index = 0,
    size_t total_matches = 0,
    const std::u16string& additional_message_format = std::u16string(),
    int* label_prefix_length = nullptr);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_
