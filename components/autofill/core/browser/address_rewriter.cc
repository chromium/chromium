// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_rewriter.h"

#include <memory>
#include <unordered_map>

#include "base/i18n/case_conversion.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {
namespace {

// Import in the internal rule table symbols. The data is defined in
// components/autofill/core/browser/address_rewriter_rules.cc
using internal::Rule;
using internal::RegionInfo;
using internal::kRuleTable;
using internal::kRuleTableSize;

// Aliases for the types used by the compiled rules cache.
using CompiledRule = std::pair<std::unique_ptr<re2::RE2>, re2::StringPiece>;
using CompiledRuleVector = std::vector<CompiledRule>;
using CompiledRuleCache = std::unordered_map<std::string, CompiledRuleVector>;

// Helper function to find the rules associated with |region|. Note that this
// requires that kRuleTable be sorted by region.
static const RegionInfo* GetRegionInfo(const base::StringPiece& region) {
  const RegionInfo* begin = kRuleTable;
  const RegionInfo* end = kRuleTable + kRuleTableSize;
  const RegionInfo* iter = std::lower_bound(begin, end, region);
  if (iter != end && region == iter->region)
    return iter;
  return nullptr;
}

// The cache of compiled string replacement rules, keyed by region. This class
// is a singleton that compiles the rules for a given region the first time
// they are requested.
class Cache {
 public:
  // Return the singleton instance of the cache.
  static Cache* GetInstance() { return base::Singleton<Cache>::get(); }

  // If the rules for |region| have already been compiled and cached, return a
  // pointer to them. Otherwise, find the rules for |region| (returning nullptr
  // if there are no such rules exist), compile them, cache them, and return a
  // pointer to the cached rules.
  const CompiledRuleVector* GetRulesForRegion(const std::string& region) {
    // Take the lock so that we don't update the data cache concurrently. Note
    // that the returned data is const and can be concurrently accessed, just
    // not the data cache.
    base::AutoLock auto_lock(lock_);

    // If we find a cached set of rules, return a pointer to the data.
    auto cache_iter = data_.find(region);
    if (cache_iter != data_.end())
      return &cache_iter->second;

    // Cache miss. Look for the raw rules. If none, then return nullptr.
    const RegionInfo* region_info = GetRegionInfo(region);
    if (region_info == nullptr)
      return nullptr;

    // Add a new rule vector the the cache and populate it with compiled rules.
    re2::RE2::Options options;
    options.set_utf8(true);
    options.set_word_boundary(true);
    CompiledRuleVector& compiled_rules = data_[region];
    compiled_rules.reserve(region_info->num_rules);
    for (size_t i = 0; i < region_info->num_rules; ++i) {
      const Rule& rule = region_info->rules[i];
      std::unique_ptr<re2::RE2> pattern(new re2::RE2(rule.pattern, options));
      re2::StringPiece rewrite(rule.rewrite);
      compiled_rules.emplace_back(std::move(pattern), std::move(rewrite));
    }

    // Return a pointer to the data.
    return &compiled_rules;
  }

 private:
  Cache() {}

  // Synchronizes access to |data_|, ensuring that a given set of rules is
  // only compiled once.
  base::Lock lock_;

  // The cache of compiled rules, keyed by region.
  CompiledRuleCache data_;

  friend struct base::DefaultSingletonTraits<Cache>;
  DISALLOW_COPY_AND_ASSIGN(Cache);
};

}  // namespace

AddressRewriter AddressRewriter::ForCountryCode(
    const base::string16& country_code) {
  const std::string region =
      base::UTF16ToUTF8(base::i18n::ToUpper(country_code));
  const CompiledRuleVector* rules =
      Cache::GetInstance()->GetRulesForRegion(region);
  AddressRewriter rewriter;
  rewriter.impl_ = rules;
  return rewriter;
}

base::string16 AddressRewriter::Rewrite(const base::string16& text) const {
  if (impl_ == nullptr)
    return base::CollapseWhitespace(text, true);

  // Apply all of the string replacement rules. We don't have to worry about
  // whitespace during these passes because the patterns are all whitespace
  // tolerant regular expressions.
  std::string utf8_text = base::UTF16ToUTF8(text);
  for (const auto& rule : *static_cast<const CompiledRuleVector*>(impl_)) {
    RE2::GlobalReplace(&utf8_text, *rule.first, rule.second);
  }

  // Collapse whitespace before returning the final value.
  return base::UTF8ToUTF16(base::CollapseWhitespaceASCII(utf8_text, true));
}

}  // namespace autofill
