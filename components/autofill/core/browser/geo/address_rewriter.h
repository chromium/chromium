// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

// A class to apply address normalization rewriting rules to strings. This
// class is a wrapper to a handle for a set of cached rules. As such, it is
// copyable, movable, and passable by value.
class AddressRewriter {
 public:
  // Rewrites |text| using the rules for |country_code|.
  static std::u16string RewriteForCountryCode(
      const AddressCountryCode& country_code,
      const std::u16string& normalized_text);

  // Get an AddressRewrite instance which applies the rules for `country_code`.
  static AddressRewriter ForCountryCode(const AddressCountryCode& country_code);

  // Gets an AddressRewrite instance for tests with custom rules.
  static AddressRewriter ForCustomRules(const std::string& custom_rules);

  // Apply the rewrite rules to |text| and return the result.
  std::u16string Rewrite(const std::u16string& text) const;

 private:
  // Aliases for the types used by the compiled rules cache.
  using CompiledRule = std::pair<std::unique_ptr<re2::RE2>, std::string>;
  using CompiledRuleVector = std::vector<CompiledRule>;
  using CompiledRuleCache = std::unordered_map<std::string, CompiledRuleVector>;

  class Cache;

  explicit AddressRewriter(const CompiledRuleVector* compiled_rules);

  static void CompileRulesFromData(const std::string& data_string,
                                   CompiledRuleVector* compiled_rules);

  // A handle to the internal rewrite rules this instance is using.
  const raw_ptr<const CompiledRuleVector> compiled_rules_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_
