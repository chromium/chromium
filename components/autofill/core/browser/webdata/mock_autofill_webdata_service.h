// Copyright 2020 The Chromium Authors
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

  MOCK_METHOD(void,
              AddFormFields,
              (const std::vector<FormFieldData>&),
              (override));
  MOCK_METHOD(void, CancelRequest, (int), (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              GetFormValuesForElementName,
              (const std::u16string& name,
               const std::u16string& prefix,
               int limit,
               WebDataServiceConsumer* consumer),
              (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              RemoveExpiredAutocompleteEntries,
              (WebDataServiceConsumer * consumer),
              (override));

 protected:
  ~MockAutofillWebDataService() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_SERVICE_H_
