// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/base/data_type.h"

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence {
 public:
  // Called on DB sequence whenever autocomplete entries are changed.
  virtual void AutocompleteEntriesChanged(
      const AutocompleteChangeList& changes) {}

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

  // Called on DB sequence when a entity instance has been added/updated/deleted
  // in the WebDatabase.
  virtual void EntityInstanceChanged(const EntityInstanceChange& change) {}

  // Called on DB sequence when a server entity instance's metadata has been
  // added/updated/deleted in the WebDatabase.
  // It's called when the metadata is changed (a) due to a change to the entity
  // (via AutofillWebdataBackendImpl), or (b) due to the entity being deleted
  // (via ValuableSyncBridge, for some reason). But it's not called when the
  // metadata is changed directly (not through the entity), by
  // ValuableMetadataSyncBridge.
  // TODO(crbug.com/436551488): Provide consistent notifications from all points
  // were server metadata is changed.
  virtual void ServerEntityInstanceMetadataChanged(
      const EntityInstanceMetadataChange& change) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnDBSequence() = default;
};

class AutofillWebDataServiceObserverOnUISequence {
 public:
  // Called on UI sequence when Autofill entries have been modified by
  // Sync. Can be called multiple times for the same `data_type`.
  virtual void OnAutofillChangedBySync(syncer::DataType data_type) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnUISequence() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
