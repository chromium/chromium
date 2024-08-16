// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include <array>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
std::string AutocompleteMatchType::ToString(AutocompleteMatchType::Type type) {
  // clang-format off
  static constexpr auto strings = std::to_array<const char*>({
    "url-what-you-typed",
    "history-url",
    "history-title",
    "history-body",
    "history-keyword",
    "navsuggest",
    "search-what-you-typed",
    "search-history",
    "search-suggest",
    "search-suggest-entity",
    "search-suggest-infinite",
    "search-suggest-personalized",
    "search-suggest-profile",
    "search-other-engine",
    "extension-app",
    "contact",
    "bookmark-title",
    "navsuggest-personalized",
    "search-calculator-answer",
    "url-from-clipboard",
    "voice-suggest",
    "physical-web",
    "physical-web-overflow",
    "tab-search",
    "document",
    "pedal",
    "text-from-clipboard",
    "image-from-clipboard",
    "query-tiles",
    "navsuggest-tiles",
    "open-tab",
    "history-cluster",
    "null-result-message",
    "starter-pack",
    "most-visited-site-tile",
    "organic-repeatable-query-tile",
    "history-embeddings",
    "featured-enterprise-search",
  });
  // clang-format on
  static_assert(strings.size() == AutocompleteMatchType::NUM_TYPES,
                "strings array must have NUM_TYPES elements");
  return strings[type];
}

// static
bool AutocompleteMatchType::FromInteger(int value, Type* result) {
  DCHECK(result);

  if (value < Type::URL_WHAT_YOU_TYPED || value >= Type::NUM_TYPES) {
    return false;
  }

  *result = static_cast<Type>(value);
  return true;
}

static constexpr char16_t kAccessibilityLabelPrefixEndSentinel[] =
    u"\uFFFC";  // Embedded object character.

static int AccessibilityLabelPrefixLength(std::u16string accessibility_label) {
  auto length = accessibility_label.find(kAccessibilityLabelPrefixEndSentinel);
  return length == std::u16string::npos ? 0 : static_cast<int>(length);
}

// Places the |replacement| inside the given format string.
// It also adjusts |label_prefix_length|, if non-nullptr.
std::u16string AddAdditionalMessaging(const std::u16string& format,
                                      const std::u16string& replacement,
                                      int* label_prefix_length) {
  if (label_prefix_length) {
    *label_prefix_length +=
        AccessibilityLabelPrefixLength(l10n_util::FormatString(
            format, {kAccessibilityLabelPrefixEndSentinel}, nullptr));
  }
  return l10n_util::FormatString(format, {replacement}, nullptr);
}

// Returns the base label for this match, without handling the positional or
// secondary button messaging.
std::u16string GetAccessibilityBaseLabel(const AutocompleteMatch& match,
                                         const std::u16string& match_text,
                                         int* label_prefix_length) {
  // Types with a message ID of zero get |text| returned as-is.
  static constexpr auto message_ids = std::to_array<int>({
      0,                             // URL_WHAT_YOU_TYPED
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_URL
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_TITLE
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_BODY

      // HISTORY_KEYWORD is a custom search engine with no %s in its string - so
      // more or less a regular URL.
      0,                                             // HISTORY_KEYWORD
      0,                                             // NAVSUGGEST
      IDS_ACC_AUTOCOMPLETE_SEARCH,                   // SEARCH_WHAT_YOU_TYPED
      IDS_ACC_AUTOCOMPLETE_SEARCH_HISTORY,           // SEARCH_HISTORY
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,         // SEARCH_SUGGEST
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH_ENTITY,  // SEARCH_SUGGEST_ENTITY
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,         // SEARCH_SUGGEST_TAIL

      // SEARCH_SUGGEST_PERSONALIZED are searches from history elsewhere, maybe
      // on other machines via Sync, or when signed in to Google.
      IDS_ACC_AUTOCOMPLETE_HISTORY,           // SEARCH_SUGGEST_PERSONALIZED
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,  // SEARCH_SUGGEST_PROFILE
      IDS_ACC_AUTOCOMPLETE_SEARCH,            // SEARCH_OTHER_ENGINE
      0,                                      // EXTENSION_APP (deprecated)
      0,                                      // CONTACT_DEPRECATED
      IDS_ACC_AUTOCOMPLETE_BOOKMARK,          // BOOKMARK_TITLE

      // NAVSUGGEST_PERSONALIZED is like SEARCH_SUGGEST_PERSONALIZED, but it's a
      // URL instead of a search query.
      IDS_ACC_AUTOCOMPLETE_HISTORY,        // NAVSUGGEST_PERSONALIZED
      0,                                   // CALCULATOR
      IDS_ACC_AUTOCOMPLETE_CLIPBOARD_URL,  // CLIPBOARD_URL
      0,                                   // VOICE_SUGGEST
      0,                                   // PHYSICAL_WEB_DEPRECATED
      0,                                   // PHYSICAL_WEB_OVERFLOW_DEPRECATED
      IDS_ACC_AUTOCOMPLETE_HISTORY,        // TAB_SEARCH_DEPRECATED
      0,                                   // DOCUMENT_SUGGESTION

      // TODO(orinj): Determine appropriate accessibility labels for Pedals
      0,                                     // PEDAL
      IDS_ACC_AUTOCOMPLETE_CLIPBOARD_TEXT,   // CLIPBOARD_TEXT
      IDS_ACC_AUTOCOMPLETE_CLIPBOARD_IMAGE,  // CLIPBOARD_IMAGE
      0,                                     // TILE_SUGGESTION
      0,                                     // TILE_NAVSUGGEST
      0,                                     // OPEN_TAB
      0,                                     // HISTORY_CLUSTER
      0,                                     // NULL_RESULT_MESSAGE
      0,                                     // STARTER_PACK
      0,                                     // TILE_MOST_VISITED_SITE
      0,                                     // TILE_REPEATABLE_QUERY
      IDS_ACC_AUTOCOMPLETE_HISTORY,          // HISTORY_EMBEDDINGS
      0,                                     // FEATURED_ENTERPRISE_SEARCH
  });
  static_assert(std::size(message_ids) == AutocompleteMatchType::NUM_TYPES,
                "message_ids must have NUM_TYPES elements");

  // Document provider should use its full display text; description has
  // already been constructed via IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE.
  // TODO(skare) http://crbug.com/951109: format as string in grd so this isn't
  // special-cased.
  if (match.type == AutocompleteMatchType::DOCUMENT_SUGGESTION) {
    std::u16string doc_string =
        match.contents + u", " + match.description + u", " + match_text;
    return doc_string;
  }

  // Standalone action suggestions must use the associated accessibility hint.
  if (match.type == AutocompleteMatchType::PEDAL) {
    DCHECK(match.takeover_action);
    return match.takeover_action->GetLabelStrings().accessibility_hint;
  }

  int message = message_ids[match.type];
  if (!message)
    return match_text;

  std::u16string description;
  bool has_description = false;
  switch (message) {
    case IDS_ACC_AUTOCOMPLETE_SEARCH_HISTORY:
    case IDS_ACC_AUTOCOMPLETE_SEARCH:
    case IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH:
      // Search match.
      // If additional descriptive text exists with a search, treat as search
      // with immediate answer, such as Weather in Boston: 53 degrees.
      if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
          match.answer_template) {
        omnibox::FormattedString subhead =
            match.answer_template->answers(0).subhead();
        description = base::UTF8ToUTF16(
            subhead.has_a11y_text() ? subhead.a11y_text() : subhead.text());
        has_description = true;
        message = IDS_ACC_AUTOCOMPLETE_QUICK_ANSWER;
      } else if (match.answer) {
        description = match.answer->second_line().AccessibleText();
        has_description = true;
        message = IDS_ACC_AUTOCOMPLETE_QUICK_ANSWER;
      }
      break;
    case IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH_ENTITY:
      if (match.description.empty()) {
        // No description, so fall back to ordinary search suggestion format.
        message = IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH;
      } else {
        // Full entity search suggestion with description.
        description = match.description;
        has_description = true;
      }
      break;
    case IDS_ACC_AUTOCOMPLETE_HISTORY:
    case IDS_ACC_AUTOCOMPLETE_BOOKMARK:
      // History match.
      // May have descriptive text for the title of the page.
      description = match.description;
      has_description = true;
      break;
    case IDS_ACC_AUTOCOMPLETE_CLIPBOARD_URL:
    case IDS_ACC_AUTOCOMPLETE_CLIPBOARD_TEXT:
      // Clipboard match.
      // Description contains clipboard content
      description = match.description;
      has_description = true;
      break;
    case IDS_ACC_AUTOCOMPLETE_CLIPBOARD_IMAGE:
      // Clipboard match with no textual clipboard content.
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Get the length of friendly text inserted before the actual suggested match.
  if (label_prefix_length) {
    *label_prefix_length =
        has_description
            ? AccessibilityLabelPrefixLength(l10n_util::GetStringFUTF16(
                  message, kAccessibilityLabelPrefixEndSentinel, description))
            : AccessibilityLabelPrefixLength(l10n_util::GetStringFUTF16(
                  message, kAccessibilityLabelPrefixEndSentinel));
  }

  return has_description
             ? l10n_util::GetStringFUTF16(message, match_text, description)
             : l10n_util::GetStringFUTF16(message, match_text);
}

// static
std::u16string AutocompleteMatchType::ToAccessibilityLabel(
    const AutocompleteMatch& match,
    const std::u16string& match_text,
    size_t match_index,
    size_t total_matches,
    const std::u16string& additional_message_format,
    int* label_prefix_length) {
  if (label_prefix_length)
    *label_prefix_length = 0;

  // Start with getting the base label.
  std::u16string result =
      GetAccessibilityBaseLabel(match, match_text, label_prefix_length);

  // Add the additional message, if applicable.
  if (!additional_message_format.empty()) {
    result = AddAdditionalMessaging(additional_message_format, result,
                                    label_prefix_length);
  }

  // Add the positional info, if applicable.
  if (total_matches != 0 && match_index != OmniboxPopupSelection::kNoMatch) {
    // TODO(tommycli): If any localization of the "n of m" positional message
    // puts it as a prefix, then |label_prefix_length| will get the wrong value.
    result = l10n_util::GetStringFUTF16(IDS_ACC_AUTOCOMPLETE_N_OF_M, result,
                                        base::NumberToString16(match_index + 1),
                                        base::NumberToString16(total_matches));
  }

  return result;
}
