// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_

#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillPlusAddressDelegate : public AutofillPlusAddressDelegate {
 public:
  MockAutofillPlusAddressDelegate();
  ~MockAutofillPlusAddressDelegate() override;

  MOCK_METHOD(bool,
              SupportsPlusAddresses,
              (const url::Origin&, bool),
              (const override));
  MOCK_METHOD(std::optional<std::string>,
              GetPlusAddress,
              (const url::Origin&),
              (const override));
  MOCK_METHOD(bool, IsPlusAddress, (const std::string&), (const override));
  MOCK_METHOD(std::u16string, GetCreateSuggestionLabel, (), (const override));
  MOCK_METHOD(void,
              RecordAutofillSuggestionEvent,
              (SuggestionEvent),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
