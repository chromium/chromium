// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_text_util.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace omnibox {

std::u16string StripJavascriptSchemas(const std::u16string& text) {
  const std::u16string kJsPrefix(
      base::StrCat({url::kJavaScriptScheme16, u":"}));

  bool found_JavaScript = false;
  size_t i = 0;
  // Find the index of the first character that isn't whitespace, a control
  // character, or a part of a JavaScript: scheme.
  while (i < text.size()) {
    if (base::IsUnicodeWhitespace(text[i]) || (text[i] < 0x20)) {
      ++i;
    } else {
      if (!base::EqualsCaseInsensitiveASCII(text.substr(i, kJsPrefix.length()),
                                            kJsPrefix)) {
        break;
      }

      // We've found a JavaScript scheme. Continue searching to ensure that
      // strings like "javascript:javascript:alert()" are fully stripped.
      found_JavaScript = true;
      i += kJsPrefix.length();
    }
  }

  // If we found any "JavaScript:" schemes in the text, return the text starting
  // at the first non-whitespace/control character after the last instance of
  // the scheme.
  if (found_JavaScript) {
    return text.substr(i);
  }

  return text;
}

std::u16string SanitizeTextForPaste(const std::u16string& text) {
  if (text.empty()) {
    return std::u16string();  // Nothing to do.
  }

  size_t end = text.find_first_not_of(base::kWhitespaceUTF16);
  if (end == std::u16string::npos) {
    return u" ";  // Convert all-whitespace to single space.
  }
  // Because `end` points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Reserve space for the sanitized output.
  std::u16string output;
  output.reserve(text.size());  // Guaranteed to be large enough.

  // Copy all non-whitespace sequences.
  // Do not copy trailing whitespace.
  // Copy all other whitespace sequences that do not contain CR/LF.
  // Convert all other whitespace sequences that do contain CR/LF to either ' '
  // or nothing, depending on whether there are any other sequences that do not
  // contain CR/LF.
  bool output_needs_lf_conversion = false;
  bool seen_non_lf_whitespace = false;
  const auto copy_range = [&text, &output](size_t begin, size_t end) {
    output +=
        text.substr(begin, (end == std::u16string::npos) ? end : (end - begin));
  };
  constexpr char16_t kNewline[] = {'\n', 0};
  constexpr char16_t kSpace[] = {' ', 0};
  while (true) {
    // Copy this non-whitespace sequence.
    size_t begin = end;
    end = text.find_first_of(base::kWhitespaceUTF16, begin + 1);
    copy_range(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end != std::u16string::npos) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      end = text.find_first_not_of(base::kWhitespaceNoCrLfUTF16, begin);
      if ((end != std::u16string::npos) && (text[end] != '\n') &&
          (text[end] != '\r')) {
        // Found a non-trailing whitespace sequence without CR/LF. Copy it.
        seen_non_lf_whitespace = true;
        copy_range(begin, end);
        continue;
      }
    }

    // `end` either points at the end of the string or a CR/LF.
    if (end != std::u16string::npos) {
      end = text.find_first_not_of(base::kWhitespaceUTF16, end + 1);
    }
    if (end == std::u16string::npos) {
      break;  // Ignore any trailing whitespace.
    }

    // The preceding whitespace sequence contained CR/LF. Convert to a single
    // LF that we'll fix up below the loop.
    output_needs_lf_conversion = true;
    output += '\n';
  }

  // Convert LFs to ' ' or '' depending on whether there were non-LF whitespace
  // sequences.
  if (output_needs_lf_conversion) {
    base::ReplaceChars(output, kNewline,
                       seen_non_lf_whitespace ? kSpace : std::u16string(),
                       &output);
  }

  return StripJavascriptSchemas(output);
}

void AdjustTextForCopy(int sel_min,
                       std::u16string* text,
                       bool has_user_modified_text,
                       bool is_keyword_selected,
                       std::optional<AutocompleteMatch> current_popup_match,
                       OmniboxClient* client,
                       GURL* url_from_text,
                       bool* write_url) {
  DCHECK(text);
  DCHECK(url_from_text);
  DCHECK(write_url);

  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field.
  if (sel_min != 0) {
    return;
  }

  // If the user has not modified the display text and is copying the whole URL
  // text (whether it's in the elided or unelided form), copy the omnibox
  // contents as a hyperlink to the current page.
  if (!has_user_modified_text) {
    *url_from_text = client->GetNavigationEntryURL();
    *write_url = true;

    // Don't let users copy Reader Mode page URLs.
    // We display the original article's URL in the omnibox, so users will
    // expect that to be what is copied to the clipboard.
    if (dom_distiller::url_utils::IsDistilledPage(*url_from_text)) {
      *url_from_text = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
          *url_from_text);
    }
    *text = base::UTF8ToUTF16(url_from_text->spec());
    return;
  }

  // This code early exits if the copied text looks like a search query. It's
  // not at the very top of this method, as it would interpret the intranet URL
  // "printer/path" as a search query instead of a URL.
  //
  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match_from_text;
  client->GetAutocompleteClassifier()->Classify(
      *text, is_keyword_selected, true,
      client->GetPageClassification(/*is_prefetch=*/false), &match_from_text,
      nullptr);
  if (AutocompleteMatch::IsSearchType(match_from_text.type)) {
    return;
  }

  // Make our best GURL interpretation of |text|.
  *url_from_text = match_from_text.destination_url;

  // Get the current page GURL (or the GURL of the currently selected match).
  GURL current_page_url = client->GetNavigationEntryURL();
  if (current_popup_match) {
    AutocompleteMatch current_match = *current_popup_match;
    if (!AutocompleteMatch::IsSearchType(current_match.type) &&
        current_match.destination_url.is_valid()) {
      // If the popup is open and a valid match is selected, treat that as the
      // current page, since the URL in the Omnibox will be from that match.
      current_page_url = current_match.destination_url;
    }
  }

  // If the user has altered the host piece of the omnibox text, then we cannot
  // guess at user intent, so early exit and leave |text| as-is as plain text.
  if (!current_page_url.SchemeIsHTTPOrHTTPS() ||
      !url_from_text->SchemeIsHTTPOrHTTPS() ||
      current_page_url.host() != url_from_text->host()) {
    return;
  }

  // Infer the correct scheme for the copied text, and prepend it if necessary.
  {
    const std::u16string http =
        base::StrCat({url::kHttpScheme16, url::kStandardSchemeSeparator16});
    const std::u16string https =
        base::StrCat({url::kHttpsScheme16, url::kStandardSchemeSeparator16});

    const std::u16string& current_page_url_prefix =
        current_page_url.SchemeIs(url::kHttpScheme) ? http : https;

    // Only prepend a scheme if the text doesn't already have a scheme.
    if (!base::StartsWith(*text, http, base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(*text, https, base::CompareCase::INSENSITIVE_ASCII)) {
      *text = current_page_url_prefix + *text;

      // Amend the copied URL to match the prefixed string.
      GURL::Replacements replace_scheme;
      replace_scheme.SetSchemeStr(current_page_url.scheme());
      *url_from_text = url_from_text->ReplaceComponents(replace_scheme);
    }
  }

  // If the URL derived from |text| is valid, mark |write_url| true, and modify
  // |text| to contain the canonical URL spec with non-ASCII characters escaped.
  if (url_from_text->is_valid()) {
    *write_url = true;
    *text = base::UTF8ToUTF16(url_from_text->spec());
  }
}

}  // namespace omnibox
