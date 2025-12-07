// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_

#include <optional>
#include <string>

struct AutocompleteMatch;
class GURL;
class OmniboxClient;

namespace omnibox {

// Returns `text` with any leading javascript schemas stripped.
std::u16string StripJavascriptSchemas(const std::u16string& text);

// Automatically collapses internal whitespace as follows:
// * Leading and trailing whitespace are often copied accidentally and rarely
//   affect behavior, so they are stripped. If this collapses the whole
//   string, returns a space, since pasting nothing feels broken.
// * Internal whitespace sequences not containing CR/LF may be integral to the
//   meaning of the string and are preserved exactly. The presence of any of
//   these also suggests the input is more likely a search than a navigation,
//   which affects the next bullet.
// * Internal whitespace sequences containing CR/LF have likely been split
//   across lines by terminals, email programs, etc., and are collapsed. If
//   there are any internal non-CR/LF whitespace sequences, the input is more
//   likely search data (e.g. street addresses), so collapse these to a single
//   space. If not, the input might be a navigation (e.g. a line-broken URL),
//   so collapse these away entirely.
//
// Finally, calls StripJavascriptSchemas() on the resulting string.
std::u16string SanitizeTextForPaste(const std::u16string& text);

// Adjusts the copied text before writing it to the clipboard. If the copied
// text is a URL with the scheme elided, this method reattaches the scheme.
// Copied text that looks like a search query will not be modified.
//
// |sel_min| gives the minimum of the selection, e.g. min(sel_start, sel_end).
// |text| is the currently selected text, and may be modified by this method.
// |url_from_text| is the GURL interpretation of the selected text, and may
// be used for drag-and-drop models or writing hyperlink data types to
// system clipboards.
//
// If the copied text is interpreted as a URL:
//  - |write_url| is set to true.
//  - |url_from_text| is set to the URL.
//  - |text| is set to the URL's spec. The output will be pure ASCII and
//    %-escaped, since canonical URLs are always encoded to ASCII.
//
// If the copied text is *NOT* interpreted as a URL:
//  - |write_url| is set to false.
//  - |url_from_text| may be modified, but might not contain a valid GURL.
//  - |text| is full UTF-16 and not %-escaped. This is because we are not
//    interpreting |text| as a URL, so we leave the Unicode characters as-is.
void AdjustTextForCopy(int sel_min,
                       std::u16string* text,
                       bool has_user_modified_text,
                       bool is_keyword_selected,
                       std::optional<AutocompleteMatch> current_popup_match,
                       OmniboxClient* client,
                       GURL* url_from_text,
                       bool* write_url);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_
