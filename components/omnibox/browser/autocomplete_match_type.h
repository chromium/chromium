// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_

#include <string>

struct AutocompleteMatch;

struct AutocompleteMatchType {
  // Type of AutocompleteMatch. Typedef'ed in autocomplete_match.h. Defined here
  // to pass the type details back and forth between the browser and renderer.
  //
  // These values are stored in ShortcutsDatabase and in GetDemotionsByType()
  // and cannot be renumbered.
  //
  // Automatically generate a corresponding Java enum:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxSuggestionType
  // clang-format off
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. The values should remain
  // synchronized with the enum AutocompleteMatchType in
  // //tools/metrics/histograms/enums.xml.
  //
  // Any changes to this enum also requires an update to:
  //  - `AutocompleteMatch::GetOmniboxEventResultType()`
  //  - `GetClientSummarizedResultType()`
  //  - `AutocompleteMatchType::ToString()`
  //  - `AutocompleteMatchType::GetAccessibilityBaseLabel()`
  enum Type {
    URL_WHAT_YOU_TYPED    = 0,  // The input as a URL.
    HISTORY_URL           = 1,  // A past page whose URL contains the input.
    HISTORY_TITLE         = 2,  // A past page whose title contains the input.
    HISTORY_BODY          = 3,  // A past page whose body contains the input.
    HISTORY_KEYWORD       = 4,  // A past page whose keyword contains the
                                // input.
    NAVSUGGEST            = 5,  // A suggested URL.
    SEARCH_WHAT_YOU_TYPED = 6,  // The input as a search query (with the
                                // default engine).
    SEARCH_HISTORY        = 7,  // A past search (with the default engine)
                                // containing the input.
    SEARCH_SUGGEST        = 8,  // A suggested search (with the default engine)
                                // query that doesn't fall into one of the more
                                // specific suggestion categories below.
    SEARCH_SUGGEST_ENTITY = 9,  // A suggested search for an entity.
    SEARCH_SUGGEST_TAIL   = 10,        // A suggested search to complete the
                                       // tail of the query.
    SEARCH_SUGGEST_PERSONALIZED = 11,  // A personalized suggested search.
    SEARCH_SUGGEST_PROFILE      = 12,  // A personalized suggested search for a
                                       // Google+ profile.
    SEARCH_OTHER_ENGINE         = 13,  // A search with a non-default engine.
    EXTENSION_APP_DEPRECATED    = 14,  // An Extension App with a title/url that
                                       // contains the input (deprecated).
    CONTACT_DEPRECATED          = 15,  // One of the user's contacts
                                       // (deprecated).
    BOOKMARK_TITLE              = 16,  // A bookmark whose title contains the
                                       // input.
    NAVSUGGEST_PERSONALIZED     = 17,  // A personalized suggestion URL.
    CALCULATOR                  = 18,  // A calculator result.
    CLIPBOARD_URL               = 19,  // A URL based on the clipboard.
    VOICE_SUGGEST               = 20,  // An Android-specific type which
                                       // indicates a search from voice
                                       // recognizer.
    PHYSICAL_WEB_DEPRECATED     = 21,  // A Physical Web nearby URL
                                       // (deprecated).
    PHYSICAL_WEB_OVERFLOW_DEPRECATED = 22,  // An item representing multiple
                                       // Physical Web nearby URLs
                                       // (deprecated).
    TAB_SEARCH_DEPRECATED       = 23,  // A suggested open tab, based on its
                                       // URL or title, via HQP (deprecated).
    DOCUMENT_SUGGESTION         = 24,  // A suggested document.
    PEDAL                       = 25,  // An omnibox pedal match.
    CLIPBOARD_TEXT              = 26,  // Text based on the clipboard.
    CLIPBOARD_IMAGE             = 27,  // An image based on the clipboard.
    TILE_SUGGESTION             = 28,  // A suggestion containing query tiles.
    TILE_NAVSUGGEST             = 29,  // A suggestion with navigation tiles.
    OPEN_TAB                    = 30,  // A URL match amongst the currently open
                                       // tabs.
    HISTORY_CLUSTER             = 31,  // A history cluster suggestion.
    NULL_RESULT_MESSAGE         = 32,  // A suggestion whose purpose is only to
                                       // deliver a message. This suggestion
                                       // cannot be opened or acted upon.
    STARTER_PACK                = 33,  // A URL suggestion that a starter pack
                                       // keyword mode chip attaches to.
    TILE_MOST_VISITED_SITE      = 34,  // Most Visited Site, shown in a
                                       // Horizontal Render Group.
                                       // Different from TILE_NAVSUGGEST which
                                       // is an aggregate type by itself.
    TILE_REPEATABLE_QUERY       = 35,  // Organic Repeatable Query, shown in a
                                       // Horizontal Render Group.
    HISTORY_EMBEDDINGS          = 36,  // A past page whose contents have
                                       // similar embeddings to the query.
    FEATURED_ENTERPRISE_SEARCH  = 37,  // Site search engines featured by
                                       // Enterprise policy.
    NUM_TYPES,
  };
  // clang-format on

  // Converts |type| to a string representation. Used in logging.
  static std::string ToString(AutocompleteMatchType::Type type);

  // Use this function to convert integers to AutocompleteMatchType enum values.
  // If you're sure it will be valid, you can call CHECK on the return value.
  // Returns true if |value| was successfully converted to a valid enum value.
  // The valid enum value will be written into |result|.
  static bool FromInteger(int value, Type* result);

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
  static std::u16string ToAccessibilityLabel(
      const AutocompleteMatch& match,
      const std::u16string& match_text,
      size_t match_index = 0,
      size_t total_matches = 0,
      const std::u16string& additional_message_format = std::u16string(),
      int* label_prefix_length = nullptr);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TYPE_H_
