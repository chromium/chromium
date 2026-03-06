// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ui_utils.h"

#include "components/search_engines/template_url.h"

namespace internal {

OrderTemplateUrlsByManagedAndAlphabetically::
    OrderTemplateUrlsByManagedAndAlphabetically() {
  UErrorCode error_code = U_ZERO_ERROR;
  collator_.reset(
      icu::Collator::createInstance(icu::Locale::getDefault(), error_code));
  if (!U_SUCCESS(error_code)) {
    collator_.reset();
  }
  if (collator_) {
    // Case-insensitive, ignoring diacriticals.
    collator_->setStrength(icu::Collator::PRIMARY);
  }
}

OrderTemplateUrlsByManagedAndAlphabetically::
    OrderTemplateUrlsByManagedAndAlphabetically(
        const OrderTemplateUrlsByManagedAndAlphabetically& other)
    : collator_(other.collator_->clone()) {}

OrderTemplateUrlsByManagedAndAlphabetically::
    ~OrderTemplateUrlsByManagedAndAlphabetically() = default;

bool OrderTemplateUrlsByManagedAndAlphabetically::operator()(
    const TemplateURL* lhs,
    const TemplateURL* rhs) const {
  auto get_sort_key = [this](const TemplateURL* engine) {
    return std::make_tuple(
        // Enterprise search engines are shown before other engines.
        !engine->CreatedByNonDefaultSearchProviderPolicy(),
        // Try to compare short names ignoring case and diacriticals.
        collator_ ? GetShortNameSortKey(engine->short_name()) : std::string(),
        // If a collator is not available, fallback to regular string
        // comparison.
        engine->short_name(),
        // If short name is the same, fallback to keyword.
        engine->keyword());
  };
  return get_sort_key(lhs) < get_sort_key(rhs);
}

std::string OrderTemplateUrlsByManagedAndAlphabetically::GetShortNameSortKey(
    const std::u16string& short_name) const {
  CHECK(collator_);

  constexpr int32_t kBufferSize = 1000;
  uint8_t buffer[kBufferSize];
  icu::UnicodeString icu_str(short_name.c_str(), short_name.length());

  int32_t sort_key_length = collator_->getSortKey(icu_str, buffer, kBufferSize);

  // If the sort key is too long for our buffer, trim the original string
  // for comparison to avoid buffer overflow.
  if (sort_key_length >= kBufferSize) {
    buffer[kBufferSize - 1] = 0;
    sort_key_length = kBufferSize;
  }

  // getSortKey returns the length including null terminator, but we want
  // to exclude it from the string to avoid issues with string comparison.
  if (sort_key_length > 0) {
    sort_key_length--;
  }

  return std::string(reinterpret_cast<const char*>(buffer), sort_key_length);
}

}  // namespace internal
