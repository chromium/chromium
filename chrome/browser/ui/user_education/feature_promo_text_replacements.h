// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_TEXT_REPLACEMENTS_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_TEXT_REPLACEMENTS_H_

#include <string>

#include "base/optional.h"

// Context-specific text replacements for an in-product help bubble's
// body text.
//
// An IPH bubble's body text is pulled from a translated string
// database. These string entries can have placeholders to be filled in
// with context-specific information, such as a number, another string,
// etc. More exotically, these placeholders can be used to add other
// inline UI such as images.
//
// FeaturePromoTextReplacements describes how to fill in these placeholders for
// IPH text. Support is currently limited to string replacements.
// Support for other replacements are a WIP.
class FeaturePromoTextReplacements {
 public:
  // Create an empty replacement pack. Use static methods below to construct
  // with replacements.
  FeaturePromoTextReplacements();

  // For a message with exactly one placeholder, fill it with a string.
  static FeaturePromoTextReplacements WithString(std::u16string s);

  FeaturePromoTextReplacements(const FeaturePromoTextReplacements&);
  FeaturePromoTextReplacements(FeaturePromoTextReplacements&&);
  ~FeaturePromoTextReplacements();

  std::u16string ApplyTo(int string_specifier) const;

 private:
  base::Optional<std::u16string> string_replacement_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_TEXT_REPLACEMENTS_H_
