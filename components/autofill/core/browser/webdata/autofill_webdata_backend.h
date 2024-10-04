// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/base/data_type.h"

class WebDatabase;

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence;

// Interface for doing Autofill work directly on the DB sequence (used by
// Sync, mostly), without fully exposing the AutofillWebDataBackend to clients.
class AutofillWebDataBackend {
 public:
  virtual ~AutofillWebDataBackend() = default;

  // Get a raw pointer to the WebDatabase.
  virtual WebDatabase* GetDatabase() = 0;

  // Add an observer to be notified of changes on the DB sequence.
  virtual void AddObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) = 0;

  // Remove an observer.
  virtual void RemoveObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) = 0;

  // Commits the currently open transaction in the database. Should be only used
  // by parties that talk directly to the database and not through the
  // WebDatabase backend (notably Sync reacting to remote changes coming from
  // the server).
  virtual void CommitChanges() = 0;

  // Notifies listeners on the DB sequence that an AutofillProfile has been
  // added/removed/updated in the WebDatabase.
  // NOTE: This method is intended to be called from the DB sequence. The UI
  // sequence notifications are asynchronous.
  virtual void NotifyOfAutofillProfileChanged(
      const AutofillProfileChange& change) = 0;

  // Notifies listeners on the DB sequence that a credit card has been
  // added/removed/updated in the WebDatabase.
  // NOTE: This method is intended to be called from the DB sequence. The UI
  // sequence notifications are asynchronous.
  virtual void NotifyOfCreditCardChanged(const CreditCardChange& change) = 0;

  // Notifies listeners on the DB sequence that an IBAN has been
  // added/removed/updated in the WebDatabase.
  // NOTE: This method is intended to be called from the DB sequence. The UI
  // sequence notifications are asynchronous.
  virtual void NotifyOfIbanChanged(const IbanChange& change) = 0;

  // Notifies listeners on UI sequences that changes have been made to
  // Autofill records of the database by the sync.
  // NOTE: The UI sequence notifications are asynchronous.
  virtual void NotifyOnAutofillChangedBySync(syncer::DataType data_type) = 0;

  // Notifies listeners on the DB sequence that a server cvc has been
  // added/removed/updated in the WebDatabase.
  // NOTE: This method is intended to be called from the DB sequence.
  virtual void NotifyOnServerCvcChanged(const ServerCvcChange& change) = 0;
};

} // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_
