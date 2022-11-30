// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_I18N_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_I18N_H_

#include "third_party/icu/source/common/unicode/uchar.h"

// Functor for a simple 16-bit Unicode case-insensitive comparison. This is
// designed for the autocomplete system where we would rather get prefix lenths
// correct than handle all possible case sensitivity issues.
//
// Any time this is used the result will be incorrect in some cases that
// certain users will be able to discern. Ideally, this class would be deleted
// and we would do full Unicode case-sensitivity mappings using
// base::i18n::ToLower. However, ToLower can change the lengths of strings,
// making computations of offsets or prefix lengths difficult. Getting all
// edge cases correct will require careful implementation and testing. In the
// mean time, we use this simpler approach.
//
// This comparator will not handle combining accents properly since it compares
// 16-bit values in isolation. If the two strings use the same sequence of
// combining accents (this is the normal case) in both strings, it will work.
//
// Additionally, this comparator does not decode UTF sequences which is why it
// is called "UCS2". UTF-16 surrogates will be compared literally (i.e. "case-
// sensitively").
//
// There are also a few cases where the lower-case version of a character
// expands to more than one code point that will not be handled properly. Such
// characters will be compared case-sensitively.
struct SimpleCaseInsensitiveCompareUCS2 {
 public:
  bool operator()(char16_t x, char16_t y) const {
    return u_tolower(x) == u_tolower(y);
  }
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_I18N_H_
