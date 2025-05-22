// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_

#include <string>

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

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TEXT_UTIL_H_
