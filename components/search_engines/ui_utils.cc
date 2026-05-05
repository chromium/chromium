// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ui_utils.h"

#include "base/check_is_test.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"

namespace internal {

namespace {

// Helper function to create and configure an icu::Collator.
std::unique_ptr<icu::Collator> CreateCollator() {
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(icu::Locale::getDefault(), error_code));
  if (!U_SUCCESS(error_code)) {
    collator.reset();
  }
  if (collator) {
    // Case-insensitive, ignoring diacriticals.
    collator->setStrength(icu::Collator::PRIMARY);
  }
  return collator;
}

// Helper to get a sort key from short name using a collator.
std::string GetShortNameSortKey(const icu::Collator& collator,
                                const std::u16string& short_name) {
  constexpr int32_t kBufferSize = 1000;
  uint8_t buffer[kBufferSize];
  icu::UnicodeString icu_str(short_name.c_str(), short_name.length());

  int32_t sort_key_length = collator.getSortKey(icu_str, buffer, kBufferSize);

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

// Helper function that returns the base sort key tuple used by
// OrderTemplateUrlsByManagedAndAlphabetically.
auto GetBaseManagedAndAlphaSortKey(const TemplateURL* engine,
                                   const icu::Collator* collator) {
  return std::make_tuple(
      // Enterprise search engines are shown before other engines.
      !engine->CreatedByNonDefaultSearchProviderPolicy(),
      // Try to compare short names ignoring case and diacriticals.
      collator ? GetShortNameSortKey(*collator, engine->short_name())
               : std::string(),
      // If a collator is not available, fallback to regular string
      // comparison.
      engine->short_name(),
      // If short name is the same, fallback to keyword.
      engine->keyword());
}

}  // namespace

// --- OrderTemplateUrlsByManagedAndAlphabetically ---
OrderTemplateUrlsByManagedAndAlphabetically::
    OrderTemplateUrlsByManagedAndAlphabetically()
    : collator_(CreateCollator()) {}

OrderTemplateUrlsByManagedAndAlphabetically::
    OrderTemplateUrlsByManagedAndAlphabetically(
        const OrderTemplateUrlsByManagedAndAlphabetically& other)
    : collator_(other.collator_->clone()) {}

OrderTemplateUrlsByManagedAndAlphabetically::
    ~OrderTemplateUrlsByManagedAndAlphabetically() = default;

bool OrderTemplateUrlsByManagedAndAlphabetically::operator()(
    const TemplateURL* lhs,
    const TemplateURL* rhs) const {
  return GetBaseManagedAndAlphaSortKey(lhs, collator_.get()) <
         GetBaseManagedAndAlphaSortKey(rhs, collator_.get());
}

std::string
OrderTemplateUrlsByManagedAndAlphabetically::GetShortNameSortKeyForTesting(
    const std::u16string& short_name) const {
  CHECK_IS_TEST();
  return GetShortNameSortKey(*collator_, short_name);
}

// --- OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically ---
OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically::
    OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
        std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines)
    : collator_(CreateCollator()) {
  prepopulated_ranks_.reserve(prepopulated_engines.size());

  // Build the `base::flat_map` mapping each `prepopulate_id` to its index.
  for (size_t i = 0; i < prepopulated_engines.size(); ++i) {
    const TemplateURLData* url = prepopulated_engines[i].get();
    CHECK(url);
    if (url->prepopulate_id > 0) {
      prepopulated_ranks_[url->prepopulate_id] = i;
    }
  }
}

OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically::
    OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
        const OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically& other)
    : collator_(other.collator_->clone()),
      prepopulated_ranks_(other.prepopulated_ranks_) {}

OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically::
    ~OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically() = default;

bool OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically::operator()(
    const TemplateURL* lhs,
    const TemplateURL* rhs) const {
  auto get_extended_sort_key = [this](const TemplateURL* engine) {
    CHECK(engine);

    // Rank the engine last by default.
    size_t prepop_rank = prepopulated_ranks_.size();

    // Get the rank based on the engine's position in `prepopulated_ranks_`.
    if (engine->prepopulate_id() > 0) {
      auto it = prepopulated_ranks_.find(engine->prepopulate_id());
      if (it != prepopulated_ranks_.end()) {
        prepop_rank = it->second;
      }
    }

    return std::tuple_cat(
        std::make_tuple(prepop_rank,
                        // If the engine is non-regional but prepopulated, rank
                        // it before custom engines.
                        !(engine->prepopulate_id() > 0)),
        GetBaseManagedAndAlphaSortKey(engine, collator_.get()));
  };

  return get_extended_sort_key(lhs) < get_extended_sort_key(rhs);
}

template_url_starter_pack_data::StarterPackIdSet GetDisabledStarterPackIds(
    bool ai_mode_enabled,
    bool gemini_enabled) {
  template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids;

  // Skip @gemini if feature disabled.
  if (!gemini_enabled) {
    disabled_starter_pack_ids.Put(
        template_url_starter_pack_data::StarterPackId::kGemini);
  }

  // Skip @page if feature disabled.
  if (!omnibox_feature_configs::ContextualSearch::Get().starter_pack_page) {
    disabled_starter_pack_ids.Put(
        template_url_starter_pack_data::StarterPackId::kPage);
  }

  // Skip @aimode if feature disabled.
  if (!ai_mode_enabled) {
    disabled_starter_pack_ids.Put(
        template_url_starter_pack_data::StarterPackId::kAiMode);
  }

  return disabled_starter_pack_ids;
}

}  // namespace internal
