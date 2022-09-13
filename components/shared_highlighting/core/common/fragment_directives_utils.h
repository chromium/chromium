// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_UTILS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_UTILS_H_

#include <vector>

#include "base/values.h"
#include "url/gurl.h"

namespace shared_highlighting {

class TextFragment;

// This file contains helper functions relating to Text Fragments, which are
// appended to the reference fragment in the URL and instruct the user agent
// to highlight a given snippet of text and the page and scroll it into view.
// See also: https://wicg.github.io/scroll-to-text-fragment/

// Splits the |url| into its |webpage_url| and |highlight_directive| parts.
// Returns true if a text fragment was present and it was extracted properly.
bool SplitUrlTextFragmentDirective(const std::string& full_url,
                                   GURL* webpage_url,
                                   std::string* highlight_directive);

// Checks the fragment portion of the URL for Text Fragments. Returns zero or
// more dictionaries containing the parsed parameters used by the fragment-
// finding algorithm, as defined in the spec.
base::Value ParseTextFragments(const GURL& url);

// Extracts the text fragments, if any, from a ref string.
std::vector<std::string> ExtractTextFragments(std::string ref_string);

// Remove fragment selector directives, if any, from url.
GURL RemoveFragmentSelectorDirectives(const GURL& url);

// Appends a set of text |fragments| with the correct format to the given
// |base_url|. Returns an empty GURL if |base_url| is invalid.
//
// Example input:
// TextFragment test_fragment("only,- start #2");
// AppendFragmentDirectives(url, {test_fragment});
GURL AppendFragmentDirectives(const GURL& base_url,
                              std::vector<TextFragment> fragments);

// Appends a set of text fragment |directives|, that have already been
// converted to an escaped string, to the given |base_url|. Returns an
// empty GURL if |base_url| is invalid.
//
// Example input:
// TextFragment test_fragment("only,- start #2");
// AppendFragmentDirectives(url, {test_fragment.ToEscapedString()});
GURL AppendFragmentDirectives(const GURL& base_url,
                              std::vector<std::string> directives);

// Appends a set of text |selectors|, the escaped strings used to identify
// a text fragment, to the given |base_url|. Returns an empty GURL
// if |base_url| is invalid.
//
// Example input:
// std::string test_selector("only%2C%2D%20start%20%232");
// AppendFragmentDirectives(url, {test_selector});
GURL AppendSelectors(const GURL& base_url, std::vector<std::string> selectors);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_UTILS_H_
