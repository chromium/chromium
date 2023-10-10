// Copyright 2013 The Chromium Authors
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

  // Called on DB sequence when an IBAN has been added/removed/updated in
  // the WebDatabase.
  virtual void IbanChanged(const IbanChange& change) {}

  // Called on DB sequence when a server CVC has been added/removed/updated in
  // the WebDatabase.
  virtual void ServerCvcChanged(const ServerCvcChange& change) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnDBSequence() {}
};

class AutofillWebDataServiceObserverOnUISequence {
 public:
  // Called on UI sequence when multiple Autofill entries have been modified by
  // Sync.
  virtual void AutofillMultipleChangedBySync(syncer::ModelType model_type) {}

  virtual void AutofillAddressConversionCompleted() {}

  virtual void AutofillProfileChanged(const AutofillProfileChange& change) {}

  // Called on the UI sequence when sync has first been enabled for
  // |model_type|. (NOT called on subsequent browser startups!)
  virtual void SyncStarted(syncer::ModelType /* model_type */) {}

  // Called after call to
  // `AutofillWebDataServiceObserverOnDBSequence::AutofillProfileChanged` on UI
  // sequence on any incremental updates when sync has been running and the
  // changes have been committed to DB.
  // Note, there is a possibility that PDM::Refresh is not finished yet, thus
  // cleanups are run on stale data, due to asynchronous calls.
  // TODO(crbug.com/1477292): Should also be called for `model_type` related to
  // payments.
  virtual void OnSyncUpdatesReceived(syncer::ModelType model_type) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnUISequence() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
