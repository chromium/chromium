// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_FORMAT_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_FORMAT_PROVIDER_H_

#include <string>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// This singleton class builds and caches the formatting expressions for value
// formatting in an AddressComponent tree. It builds the foundation for
// acquiring expressions from different countries.
class StructuredAddressesFormatProvider {
 public:
  StructuredAddressesFormatProvider& operator=(
      const StructuredAddressesFormatProvider&) = delete;
  StructuredAddressesFormatProvider(const StructuredAddressesFormatProvider&) =
      delete;
  ~StructuredAddressesFormatProvider() = delete;

  struct ContextInfo {
    bool name_has_cjk_characteristics;
  };

  // Returns a singleton instance of this class.
  static StructuredAddressesFormatProvider* GetInstance();

  // Returns the formatting expression corresponding to the provided parameters.
  // If the expression can't be found, an empty string is returned.
  std::u16string GetPattern(FieldType type,
                            std::string_view country_code,
                            const ContextInfo& info = ContextInfo()) const;

 private:
  StructuredAddressesFormatProvider();

  // Since the constructor is private, |base::NoDestructor| must be a friend to
  // be allowed to construct the cache.
  friend class base::NoDestructor<StructuredAddressesFormatProvider>;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_FORMAT_PROVIDER_H_
