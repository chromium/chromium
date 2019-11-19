// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/sync/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class WebDatabase;

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence;

class MockAutofillWebDataBackend : public AutofillWebDataBackend {
 public:
  MockAutofillWebDataBackend();
  ~MockAutofillWebDataBackend() override;

  MOCK_METHOD0(GetDatabase, WebDatabase*());
  MOCK_METHOD1(AddObserver,
               void(AutofillWebDataServiceObserverOnDBSequence* observer));
  MOCK_METHOD1(RemoveObserver,
               void(AutofillWebDataServiceObserverOnDBSequence* observer));
  MOCK_METHOD0(CommitChanges, void());
  MOCK_METHOD1(NotifyOfAutofillProfileChanged,
               void(const AutofillProfileChange& change));
  MOCK_METHOD1(NotifyOfCreditCardChanged, void(const CreditCardChange& change));
  MOCK_METHOD0(NotifyOfMultipleAutofillChanges, void());
  MOCK_METHOD0(NotifyOfAddressConversionCompleted, void());
  MOCK_METHOD1(NotifyThatSyncHasStarted, void(syncer::ModelType model_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillWebDataBackend);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_MOCK_AUTOFILL_WEBDATA_BACKEND_H_
