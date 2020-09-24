// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
std::string AutocompleteMatchType::ToString(AutocompleteMatchType::Type type) {
  // clang-format off
  const char* strings[] = {
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
  };
  // clang-format on
  static_assert(base::size(strings) == AutocompleteMatchType::NUM_TYPES,
                "strings array must have NUM_TYPES elements");
  return strings[type];
}

static const wchar_t kAccessibilityLabelPrefixEndSentinal[] =
    L"\uFFFC";  // Embedded object character.

static int AccessibilityLabelPrefixLength(base::string16 accessibility_label) {
  const base::string16 sentinal =
      base::WideToUTF16(kAccessibilityLabelPrefixEndSentinal);
  auto length = accessibility_label.find(sentinal);
  return length == base::string16::npos ? 0 : static_cast<int>(length);
}

// Gets the localized string |message_id| and places the |replacement| inside.
// It also adjusts |label_prefix_length|, if non-nullptr.
base::string16 AddAdditionalMessaging(int message_id,
                                      base::string16 replacement,
                                      int* label_prefix_length) {
  DCHECK(message_id != 0);
  if (label_prefix_length) {
    const base::string16 sentinal =
        base::WideToUTF16(kAccessibilityLabelPrefixEndSentinal);
    *label_prefix_length += AccessibilityLabelPrefixLength(
        l10n_util::GetStringFUTF16(message_id, sentinal));
  }
  return l10n_util::GetStringFUTF16(message_id, replacement);
}

// Returns the base label for this match, without handling the positional or
// secondary button messaging.
base::string16 GetAccessibilityBaseLabel(const AutocompleteMatch& match,
                                         const base::string16& match_text,
                                         int* label_prefix_length) {
  // Types with a message ID of zero get |text| returned as-is.
  static constexpr int message_ids[] = {
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
  };
  static_assert(base::size(message_ids) == AutocompleteMatchType::NUM_TYPES,
                "message_ids must have NUM_TYPES elements");

  // Document provider should use its full display text; description has
  // already been constructed via IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE.
  // TODO(skare) http://crbug.com/951109: format as string in grd so this isn't
  // special-cased.
  if (match.type == AutocompleteMatchType::DOCUMENT_SUGGESTION) {
    base::string16 doc_string = match.contents + base::ASCIIToUTF16(", ") +
                                match.description + base::ASCIIToUTF16(", ") +
                                match_text;
    return doc_string;
  }

  int message = message_ids[match.type];
  if (!message)
    return match_text;

  const base::string16 sentinal =
      base::WideToUTF16(kAccessibilityLabelPrefixEndSentinal);
  base::string16 description;
  bool has_description = false;
  switch (message) {
    case IDS_ACC_AUTOCOMPLETE_SEARCH_HISTORY:
    case IDS_ACC_AUTOCOMPLETE_SEARCH:
    case IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH:
      // Search match.
      // If additional descriptive text exists with a search, treat as search
      // with immediate answer, such as Weather in Boston: 53 degrees.
      if (match.answer) {
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
      NOTREACHED();
      break;
  }

  // Get the length of friendly text inserted before the actual suggested match.
  if (label_prefix_length) {
    *label_prefix_length =
        has_description
            ? AccessibilityLabelPrefixLength(
                  l10n_util::GetStringFUTF16(message, sentinal, description))
            : AccessibilityLabelPrefixLength(
                  l10n_util::GetStringFUTF16(message, sentinal));
  }

  return has_description
             ? l10n_util::GetStringFUTF16(message, match_text, description)
             : l10n_util::GetStringFUTF16(message, match_text);
}

// static
base::string16 AutocompleteMatchType::ToAccessibilityLabel(
    const AutocompleteMatch& match,
    const base::string16& match_text,
    size_t match_index,
    size_t total_matches,
    int additional_message_id,
    int* label_prefix_length) {
  if (label_prefix_length)
    *label_prefix_length = 0;

  // Start with getting the base label.
  base::string16 result =
      GetAccessibilityBaseLabel(match, match_text, label_prefix_length);

  // Add the additional message, if applicable.
  if (additional_message_id != 0) {
    result = AddAdditionalMessaging(additional_message_id, result,
                                    label_prefix_length);
  }

  // Add the positional info, if applicable.
  if (total_matches != 0 && match_index != OmniboxPopupModel::kNoMatch) {
    // TODO(tommycli): If any localization of the "n of m" positional message
    // puts it as a prefix, then |label_prefix_length| will get the wrong value.
    result = l10n_util::GetStringFUTF16(IDS_ACC_AUTOCOMPLETE_N_OF_M, result,
                                        base::NumberToString16(match_index + 1),
                                        base::NumberToString16(total_matches));
  }

  return result;
}
