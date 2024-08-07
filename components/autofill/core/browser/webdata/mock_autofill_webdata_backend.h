// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/sync/base/data_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class WebDatabase;

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence;

class MockAutofillWebDataBackend : public AutofillWebDataBackend {
 public:
  MockAutofillWebDataBackend();

  MockAutofillWebDataBackend(const MockAutofillWebDataBackend&) = delete;
  MockAutofillWebDataBackend& operator=(const MockAutofillWebDataBackend&) =
      delete;

  ~MockAutofillWebDataBackend() override;

  MOCK_METHOD(WebDatabase*, GetDatabase, (), (override));
  MOCK_METHOD(void,
              AddObserver,
              (AutofillWebDataServiceObserverOnDBSequence * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (AutofillWebDataServiceObserverOnDBSequence * observer),
              (override));
  MOCK_METHOD(void, CommitChanges, (), (override));
  MOCK_METHOD(void,
              NotifyOfAutofillProfileChanged,
              (const AutofillProfileChange& change),
              (override));
  MOCK_METHOD(void,
              NotifyOfCreditCardChanged,
              (const CreditCardChange& change),
              (override));
  MOCK_METHOD(void,
              NotifyOfIbanChanged,
              (const IbanChange& change),
              (override));
  MOCK_METHOD(void,
              NotifyOnAutofillChangedBySync,
              (syncer::DataType data_type),
              (override));
  MOCK_METHOD(void,
              NotifyOnServerCvcChanged,
              (const ServerCvcChange& change),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_
