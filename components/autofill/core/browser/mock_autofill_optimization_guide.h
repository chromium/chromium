// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_H_

#include "components/autofill/core/browser/autofill_optimization_guide.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill {

class MockAutofillOptimizationGuide : public AutofillOptimizationGuide {
 public:
  MockAutofillOptimizationGuide();
  ~MockAutofillOptimizationGuide() override;

  MOCK_METHOD(void,
              OnDidParseForm,
              (const FormStructure&, const PersonalDataManager*),
              (override));
  MOCK_METHOD(bool,
              ShouldBlockSingleFieldSuggestions,
              (const GURL&, AutofillField*),
              (const override));
  MOCK_METHOD(bool,
              ShouldBlockFormFieldSuggestion,
              (const GURL&, const CreditCard*),
              (const override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_H_
