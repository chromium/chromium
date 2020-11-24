// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_promo_text_replacements.h"

#include <utility>

#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"

FeaturePromoTextReplacements::FeaturePromoTextReplacements() = default;
FeaturePromoTextReplacements::FeaturePromoTextReplacements(
    const FeaturePromoTextReplacements&) = default;
FeaturePromoTextReplacements::FeaturePromoTextReplacements(
    FeaturePromoTextReplacements&&) = default;
FeaturePromoTextReplacements::~FeaturePromoTextReplacements() = default;

// static
FeaturePromoTextReplacements FeaturePromoTextReplacements::WithString(
    base::string16 s) {
  FeaturePromoTextReplacements result;
  result.string_replacement_ = std::move(s);
  return result;
}

base::string16 FeaturePromoTextReplacements::ApplyTo(
    int string_specifier) const {
  if (string_replacement_)
    return l10n_util::GetStringFUTF16(string_specifier, *string_replacement_);
  else
    return l10n_util::GetStringUTF16(string_specifier);
}
