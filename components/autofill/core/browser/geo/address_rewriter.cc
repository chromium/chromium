// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/address_rewriter.h"

#include <memory>
#include <string_view>
#include <unordered_map>

#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/grit/autofill_address_rewriter_resources_map.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {
namespace {

// Aliases for the types used by the compiled rules cache.
using CompiledRule = std::pair<std::unique_ptr<re2::RE2>, std::string>;
using CompiledRuleVector = std::vector<CompiledRule>;
using CompiledRuleCache = std::unordered_map<std::string, CompiledRuleVector>;

// Helper function to convert region to mapping key string.
std::string GetMapKey(const std::string& region) {
  return base::StrCat({"IDR_ADDRESS_REWRITER_", region, "_RULES"});
}

// Helper function to extract region rules data into |out_data|.
static bool ExtractRegionRulesData(const std::string& region,
                                   std::string* out_data) {
  int resource_id = 0;
  std::string resource_key = GetMapKey(region);
  for (size_t i = 0; i < kAutofillAddressRewriterResourcesSize; ++i) {
    // TODO: crbug.com/347651465: GRIT should define std::arrays instead of
    // c-style arrays.
    UNSAFE_TODO(if (kAutofillAddressRewriterResources[i].path == resource_key) {
      resource_id = kAutofillAddressRewriterResources[i].id;
      break;
    })
  }

  if (!resource_id) {
    return false;
  }

  // Gets and uncompresses resource data.
  std::string_view raw_resource =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  compression::GzipUncompress(raw_resource, out_data);

  return true;
}

// Helper function to populate |compiled_rules| by parsing |data_string|.
void CompileRulesFromData(const std::string& data_string,
                          CompiledRuleVector* compiled_rules) {
  std::string_view data = data_string;
  re2::RE2::Options options;
  options.set_encoding(RE2::Options::EncodingUTF8);
  options.set_word_boundary(true);

  size_t token_end = 0;
  while (!data.empty()) {
    token_end = data.find('\t');
    auto pattern =
        std::make_unique<re2::RE2>(data.substr(0, token_end), options);
    data.remove_prefix(token_end + 1);

    token_end = data.find('\n');
    std::string rewrite_string(data.substr(0, token_end));
    compiled_rules->emplace_back(std::move(pattern), std::move(rewrite_string));
    data.remove_prefix(token_end + 1);
  }
}

// The cache of compiled string replacement rules, keyed by region. This class
// is a singleton that compiles the rules for a given region the first time
// they are requested.
class Cache {
 public:
  // Return the singleton instance of the cache.
  static Cache* GetInstance() {
    static base::NoDestructor<Cache> instance;
    return instance.get();
  }

  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

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
    if (cache_iter != data_.end()) {
      return &cache_iter->second;
    }

    // Cache miss. Look for the raw rules. If none, then return nullptr.
    std::string region_rules;
    bool region_found = ExtractRegionRulesData(region, &region_rules);

    if (!region_found) {
      return nullptr;
    }

    // Add a new rule vector to the cache and populate it with compiled rules.
    CompiledRuleVector& compiled_rules = data_[region];
    CompileRulesFromData(region_rules, &compiled_rules);

    // Return a pointer to the data.
    return &compiled_rules;
  }

  // Uses a string of data to create and return a pointer to a
  // CompiledRuleVector. Used for creating unit_tests.
  const CompiledRuleVector* CreateRulesForData(const std::string& data) {
    // Compiled rules vector must be kept in cache to be used elsewhere.
    CompiledRuleVector& compiled_rules = data_[data];
    CompileRulesFromData(data, &compiled_rules);

    // Return a pointer to the data.
    return &compiled_rules;
  }

 private:
  Cache() = default;

  // Synchronizes access to |data_|, ensuring that a given set of rules is
  // only compiled once.
  base::Lock lock_;

  // The cache of compiled rules, keyed by region.
  CompiledRuleCache data_;

  friend class base::NoDestructor<Cache>;
};

}  // namespace

// static
std::u16string AddressRewriter::RewriteForCountryCode(
    const AddressCountryCode& country_code,
    const std::u16string& normalized_text) {
  AddressRewriter rewriter = AddressRewriter::ForCountryCode(country_code);
  return rewriter.Rewrite(normalized_text);
}

// static
AddressRewriter AddressRewriter::ForCountryCode(
    const AddressCountryCode& country_code) {
  const std::string region = base::ToUpperASCII(country_code.value());
  const CompiledRuleVector* rules =
      Cache::GetInstance()->GetRulesForRegion(region);
  AddressRewriter rewriter;
  rewriter.impl_ = rules;
  return rewriter;
}

// static
AddressRewriter AddressRewriter::ForCustomRules(
    const std::string& custom_rules) {
  const CompiledRuleVector* rules =
      Cache::GetInstance()->CreateRulesForData(custom_rules);
  AddressRewriter rewriter;
  rewriter.impl_ = rules;
  return rewriter;
}

std::u16string AddressRewriter::Rewrite(const std::u16string& text) const {
  if (impl_ == nullptr) {
    return base::CollapseWhitespace(text, true);
  }

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
