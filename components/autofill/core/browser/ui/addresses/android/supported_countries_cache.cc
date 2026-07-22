// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/android/supported_countries_cache.h"

#include <string>
#include <utility>

#include "base/check.h"

namespace autofill {

SupportedCountriesCache::SupportedCountriesCache(Builder builder)
    : builder_(std::move(builder)) {
  CHECK(builder_);
}

SupportedCountriesCache::~SupportedCountriesCache() = default;

std::vector<DropdownKeyValueAndroid> SupportedCountriesCache::GetForLocale(
    std::string_view locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto it = cache_.find(locale); it != cache_.end()) {
    return it->second;
  }
  return cache_[std::string(locale)] = builder_.Run(locale);
}

}  // namespace autofill
