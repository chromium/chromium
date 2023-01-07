// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_URL_PREFIX_H_
#define COMPONENTS_OMNIBOX_BROWSER_URL_PREFIX_H_

#include <stddef.h>

#include <string>
#include <vector>


struct URLPrefix;
typedef std::vector<URLPrefix> URLPrefixes;

// A URL prefix; combinations of schemes and (least significant) domain labels
// that may be inferred from certain URL-like input strings.
struct URLPrefix {
  URLPrefix(const std::u16string& prefix, size_t num_components);

  // Returns a vector of URL prefixes sorted by descending number of components.
  static const URLPrefixes& GetURLPrefixes();

  // Returns the URL prefix of |text| with the most components, or NULL.
  // |prefix_suffix| (which may be empty) is appended to every attempted prefix,
  // which is useful for finding the innermost match of user input in a URL.
  // Performs case insensitive string comparison.
  static const URLPrefix* BestURLPrefix(const std::u16string& text,
                                        const std::u16string& prefix_suffix);

  // Sees if |text| is inlineable against either |input| or |fixed_up_input|,
  // returning the appropriate inline autocomplete offset or
  // std::u16string::npos if |text| is not inlineable.
  // |allow_www_prefix_without_scheme| says whether to consider an input such
  // as "foo" to be allowed to match against text "www.foo.com".  This is
  // needed because sometimes the string we're matching against here can come
  // from a match's fill_into_edit, which can start with "www." without having
  // a protocol at the beginning, and we want to allow these matches to be
  // inlineable.  ("www." is not otherwise on the default prefix list.)
  static size_t GetInlineAutocompleteOffset(
      const std::u16string& input,
      const std::u16string& fixed_up_input,
      const bool allow_www_prefix_without_scheme,
      const std::u16string& text);

  std::u16string prefix;

  // The number of URL components (scheme, domain label, etc.) in the prefix.
  // For example, "http://foo.com" and "www.bar.com" each have one component,
  // while "ftp://ftp.ftp.com" has two, and "mysite.com" has none.
  size_t num_components;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_URL_PREFIX_H_
