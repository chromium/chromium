// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/country_type.h"

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
  // A handle to the internal rewrite rules this instance is using.
  raw_ptr<const void> impl_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_REWRITER_H_
