// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_SERVICE_H_

#include <string>

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillWebDataService : public AutofillWebDataService {
 public:
  MockAutofillWebDataService();

  MOCK_METHOD1(AddFormFields, void(const std::vector<FormFieldData>&));
  MOCK_METHOD1(CancelRequest, void(int));
  MOCK_METHOD4(GetFormValuesForElementName,
               WebDataServiceBase::Handle(const std::u16string& name,
                                          const std::u16string& prefix,
                                          int limit,
                                          WebDataServiceConsumer* consumer));
  MOCK_METHOD1(RemoveExpiredAutocompleteEntries,
               WebDataServiceBase::Handle(WebDataServiceConsumer* consumer));

 protected:
  ~MockAutofillWebDataService() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_SERVICE_H_
