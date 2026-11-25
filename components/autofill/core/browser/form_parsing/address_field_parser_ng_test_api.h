// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_TEST_API_H_

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser_ng.h"
#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"

namespace autofill {

class AddressFieldParserNGTestApi {
 public:
  explicit AddressFieldParserNGTestApi(const AddressFieldParserNG& parser)
      : parser_(parser) {}

  static std::unique_ptr<AddressFieldParserNG> Create(
      AddressCountryCode country_code) {
    return base::WrapUnique(new AddressFieldParserNG(std::move(country_code)));
  }

  const AddressHierarchyConstraints& constraints() const {
    return *parser_->constraints_;
  }

 private:
  const raw_ref<const AddressFieldParserNG> parser_;
};

inline AddressFieldParserNGTestApi test_api(
    const AddressFieldParserNG& parser) {
  return AddressFieldParserNGTestApi(parser);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_TEST_API_H_
