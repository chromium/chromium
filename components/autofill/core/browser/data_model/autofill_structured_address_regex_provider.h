// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_REGEX_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_REGEX_PROVIDER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

// Enumeration of all regular expressions supported for matching and parsing
// values in an AddressComponent tree.
enum class RegEx {
  kSingleWord,
  kParseSeparatedCjkName,
  kParseCommonCjkTwoCharacterLastName,
  kParseKoreanTwoCharacterLastName,
  kParseCjkSingleCharacterLastName,
  kMatchCjkNameCharacteristics,
  kMatchHispanicCommonNameCharacteristics,
  kMatchHispanicLastNameConjuctionCharacteristics,
  kParseOnlyLastName,
  kParseLastCommaFirstMiddleName,
  kParseFirstMiddleLastName,
  kParseHispanicLastName,
  kParseHispanicFullName,
  kParseLastNameIntoSecondLastName,
  kMatchMiddleNameInitialsCharacteristics,
  kParseStreetNameHouseNumber,
  kParseStreetNameHouseNumberSuffixedFloor,
  kParseStreetNameHouseNumberSuffixedFloorAndApartmentRe,
  kParseHouseNumberStreetName,
  kLastRegEx = kParseHouseNumberStreetName,
};

// This singleton class builds and caches the regular expressions for value
// parsing and characterization of values in an AddressComponent tree.
// It also builds the foundation for acquiring expressions from different
// sources.
class StructuredAddressesRegExProvider {
 public:
  StructuredAddressesRegExProvider& operator=(
      const StructuredAddressesRegExProvider&) = delete;
  StructuredAddressesRegExProvider(const StructuredAddressesRegExProvider&) =
      delete;
  ~StructuredAddressesRegExProvider() = delete;

  // Returns a singleton instance of this class.
  static StructuredAddressesRegExProvider* Instance();

  // Returns the regular expression corresponding to |expression_identifier|.
  // If a |country_code| is provided, the country specific instance of
  // |expression_identifier| is fetched. In case the expression is not cached
  // yet, it is built by calling |BuildRegEx(expression_identifier,
  // country_code)|. If the expression can't be built, nullptr is returned.
  const RE2* GetRegEx(RegEx expression_identifier,
                      const std::string& country_code = "");

#if UNIT_TEST
  bool IsCachedForTesting(RegEx expression_identifier) {
    return cached_expressions_.count(expression_identifier) > 0;
  }
#endif

 private:
  StructuredAddressesRegExProvider();

  // Since the constructor is private, |base::NoDestructor| must be a friend to
  // be allowed to construct the cache.
  friend class base::NoDestructor<StructuredAddressesRegExProvider>;

  // Fetches a pattern identified by |expression_identifier| and |country_code|.
  // This method is virtual and is meant to be overridden by future
  // implementations that utilize multiple sources for retrieving patterns.
  virtual std::string GetPattern(RegEx expression_identifier,
                                 const std::string& country_code);

  // A map to store already compiled enumerated expressions keyed by
  // |RegEx|.
  base::flat_map<RegEx, std::unique_ptr<const RE2>> cached_expressions_;

  // A lock to prevent concurrent access to the cached expressions map.
  base::Lock lock_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_REGEX_PROVIDER_H_
