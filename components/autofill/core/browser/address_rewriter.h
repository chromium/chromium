// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_REWRITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_REWRITER_H_

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace autofill {

// A class to apply address normalization rewriting rules to strings. This
// class is a wrapper to a handle for a set of cached rules. As such, it is
// copyable, movable, and passable by value.
class AddressRewriter {
 public:
  // Get an AddressRewrite instance which applies the rules for |country_code|.
  static AddressRewriter ForCountryCode(const base::string16& country_code);

  // Gets an AddressRewrite instance for tests with custom rules.
  static AddressRewriter ForCustomRules(const std::string& custom_rules);

  // Apply the rewrite rules to |text| and return the result.
  base::string16 Rewrite(const base::string16& text) const;

 private:
  // A handle to the internal rewrite rules this instance is using.
  const void* impl_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_REWRITER_H_
