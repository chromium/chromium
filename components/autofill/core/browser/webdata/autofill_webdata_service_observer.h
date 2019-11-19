// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/base/model_type.h"

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence {
 public:
  // Called on DB sequence whenever Autofill entries are changed.
  virtual void AutofillEntriesChanged(const AutofillChangeList& changes) {}

  // Called on DB sequence when an AutofillProfile has been
  // added/removed/updated in the WebDatabase.
  virtual void AutofillProfileChanged(const AutofillProfileChange& change) {}

  // Called on DB sequence when a CreditCard has been added/removed/updated in
  // the WebDatabase.
  virtual void CreditCardChanged(const CreditCardChange& change) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnDBSequence() {}
};

class AutofillWebDataServiceObserverOnUISequence {
 public:
  // Called on UI sequence when multiple Autofill entries have been modified by
  // Sync.
  virtual void AutofillMultipleChangedBySync() {}

  virtual void AutofillAddressConversionCompleted() {}

  virtual void AutofillProfileChanged(const AutofillProfileChange& change) {}

  // Called on UI sequence when sync has started for |model_type|.
  virtual void SyncStarted(syncer::ModelType /* model_type */) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnUISequence() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
