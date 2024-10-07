// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

namespace base {
class SequencedTaskRunner;
}

class WebDatabaseBackend;

namespace autofill {

class AutofillWebDataServiceObserverOnDBSequence;
class CreditCard;
class Iban;

// Backend implementation for the AutofillWebDataService. This class runs on the
// DB sequence, as it handles reads and writes to the WebDatabase, and functions
// in it should only be called from that sequence. Most functions here are just
// the implementations of the corresponding functions in the Autofill
// WebDataService.
// This class is destroyed on the DB sequence.
class AutofillWebDataBackendImpl
    : public base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>,
      public AutofillWebDataBackend {
 public:
  // `web_database_backend` is used to access the WebDatabase directly for
  // Sync-related operations. `ui_task_runner` and `db_task_runner` are the task
  // runners that this class uses for UI and DB tasks respectively.
  // `on_autofill_changed_by_sync_callback_` is a closure which can be used to
  // notify the UI sequence of changes initiated by Sync (this callback may be
  // called multiple times).
  AutofillWebDataBackendImpl(
      scoped_refptr<WebDatabaseBackend> web_database_backend,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      const base::RepeatingCallback<void(syncer::DataType)>&
          on_autofill_changed_by_sync_callback);

  AutofillWebDataBackendImpl(const AutofillWebDataBackendImpl&) = delete;
  AutofillWebDataBackendImpl& operator=(const AutofillWebDataBackendImpl&) =
      delete;

  void SetAutofillProfileChangedCallback(
      base::RepeatingCallback<void(const AutofillProfileChange&)> change_cb);

  // AutofillWebDataBackend implementation.
  void AddObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) override;
  void RemoveObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) override;
  WebDatabase* GetDatabase() override;
  void NotifyOfAutofillProfileChanged(
      const AutofillProfileChange& change) override;
  void NotifyOfCreditCardChanged(const CreditCardChange& change) override;
  void NotifyOfIbanChanged(const IbanChange& change) override;
  void NotifyOnAutofillChangedBySync(syncer::DataType data_type) override;
  void NotifyOnServerCvcChanged(const ServerCvcChange& change) override;
  void CommitChanges() override;

  // Returns a SupportsUserData object that may be used to store data accessible
  // from the DB sequence. Should be called only from the DB sequence, and will
  // be destroyed on the DB sequence soon after ShutdownOnUISequence() is
  // called.
  base::SupportsUserData* GetDBUserData();

  void ResetUserData();

  // Adds form fields to the web database.
  WebDatabase::State AddFormElements(const std::vector<FormFieldData>& fields,
                                     WebDatabase* db);

  // Returns a vector of values which have been entered in form input fields
  // named |name|.
  std::unique_ptr<WDTypedResult> GetFormValuesForElementName(
      const std::u16string& name,
      const std::u16string& prefix,
      int limit,
      WebDatabase* db);

  // Function to remove expired Autocomplete entries, which deletes them from
  // the Sqlite table, unlinks them from Sync and cleans up the metadata.
  // Returns the number of entries cleaned-up.
  std::unique_ptr<WDTypedResult> RemoveExpiredAutocompleteEntries(
      WebDatabase* db);

  // Removes form elements recorded for Autocomplete from the database.
  WebDatabase::State RemoveFormElementsAddedBetween(base::Time delete_begin,
                                                    base::Time delete_end,
                                                    WebDatabase* db);

  // Removes the Form-value |value| which has been entered in form input fields
  // named |name| from the database.
  WebDatabase::State RemoveFormValueForElementName(const std::u16string& name,
                                                   const std::u16string& value,
                                                   WebDatabase* db);

  // Adds an Autofill profile to the web database.
  WebDatabase::State AddAutofillProfile(const AutofillProfile& profile,
                                        WebDatabase* db);

  // Updates an Autofill profile in the web database.
  WebDatabase::State UpdateAutofillProfile(const AutofillProfile& profile,
                                           WebDatabase* db);

  // Removes an Autofill profile from the web database.
  WebDatabase::State RemoveAutofillProfile(const std::string& guid,
                                           WebDatabase* db);

  // Returns the Autofill profiles from the web database.
  std::unique_ptr<WDTypedResult> GetAutofillProfiles(
      WebDatabase* db);

  // Returns the number of values such that all for autofill entries with that
  // value, the interval between creation date and last usage is entirely
  // contained between [|begin|, |end|).
  std::unique_ptr<WDTypedResult> GetCountOfValuesContainedBetween(
      base::Time begin,
      base::Time end,
      WebDatabase* db);

  // Updates autocomplete entries in the web database.
  WebDatabase::State UpdateAutocompleteEntries(
      const std::vector<AutocompleteEntry>& autocomplete_entries,
      WebDatabase* db);

  // Adds a credit card to the web database. Valid only for local cards.
  WebDatabase::State AddCreditCard(const CreditCard& credit_card,
                                   WebDatabase* db);

  // Updates a credit card in the web database. Valid only for local cards.
  WebDatabase::State UpdateCreditCard(const CreditCard& credit_card,
                                      WebDatabase* db);

  // Updates a local CVC in the web database.
  WebDatabase::State UpdateLocalCvc(const std::string& guid,
                                    const std::u16string& cvc,
                                    WebDatabase* db);

  // Removes a credit card from the web database. Valid only for local cards.
  WebDatabase::State RemoveCreditCard(const std::string& guid, WebDatabase* db);

  // Returns a vector of local/server credit cards from the web database.
  std::unique_ptr<WDTypedResult> GetCreditCards(WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetServerCreditCards(WebDatabase* db);

  // Returns a vector of local/server IBANs from the web database.
  std::unique_ptr<WDTypedResult> GetLocalIbans(WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetServerIbans(WebDatabase* db);

  // Adds an IBAN to the web database.
  WebDatabase::State AddLocalIban(const Iban& iban, WebDatabase* db);

  // Updates an IBAN in the web database.
  WebDatabase::State UpdateLocalIban(const Iban& iban, WebDatabase* db);

  // Removes an IBAN from the web database.
  WebDatabase::State RemoveLocalIban(const std::string& guid, WebDatabase* db);

  // Updates the given `iban`'s metadata in the web database.
  WebDatabase::State UpdateServerIbanMetadata(const Iban& iban,
                                              WebDatabase* db);

  WebDatabase::State UpdateServerCardMetadata(const CreditCard& credit_card,
                                              WebDatabase* db);

  // Methods to add, update, remove, clear server cvc in the web database.
  WebDatabase::State AddServerCvc(int64_t instrument_id,
                                  const std::u16string& cvc,
                                  WebDatabase* db);
  WebDatabase::State UpdateServerCvc(int64_t instrument_id,
                                     const std::u16string& cvc,
                                     WebDatabase* db);
  WebDatabase::State RemoveServerCvc(int64_t instrument_id, WebDatabase* db);
  WebDatabase::State ClearServerCvcs(WebDatabase* db);

  // Method to clear all the local CVCs from the web database.
  WebDatabase::State ClearLocalCvcs(WebDatabase* db);

  // Returns the PaymentsCustomerData from the database.
  std::unique_ptr<WDTypedResult> GetPaymentsCustomerData(WebDatabase* db);

  // Returns the CreditCardCloudTokenData from the database.
  std::unique_ptr<WDTypedResult> GetCreditCardCloudTokenData(WebDatabase* db);

  // Returns Credit Card Offers Data from the database.
  std::unique_ptr<WDTypedResult> GetAutofillOffers(WebDatabase* db);

  // Returns Virtual Card Usage Data from the database.
  std::unique_ptr<WDTypedResult> GetAutofillVirtualCardUsageData(
      WebDatabase* db);

  // Returns all Credit Card Benefits from the database.
  std::unique_ptr<WDTypedResult> GetCreditCardBenefits(WebDatabase* db);

  // Returns a vector of masked bank accounts from the web database.
  std::unique_ptr<WDTypedResult> GetMaskedBankAccounts(WebDatabase* db);

  // Returns a vector of payment instruments from the web database.
  std::unique_ptr<WDTypedResult> GetPaymentInstruments(WebDatabase* db);

  WebDatabase::State ClearAllServerData(WebDatabase* db);

  // Clears all the credit card benefits from the database.
  WebDatabase::State ClearAllCreditCardBenefits(WebDatabase* db);

  // Adds a server credit card to the web database. Used only in tests - in
  // production, server cards are set directly from Chrome Sync code.
  WebDatabase::State AddServerCreditCardForTesting(
      const CreditCard& credit_card,
      WebDatabase* db);

 protected:
  ~AutofillWebDataBackendImpl() override;

 private:
  friend class base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>;
  friend class base::DeleteHelper<AutofillWebDataBackendImpl>;

  // This makes the destructor public, and thus allows us to aggregate
  // SupportsUserData. It is private by default to prevent incorrect
  // usage in class hierarchies where it is inherited by
  // reference-counted objects.
  class SupportsUserDataAggregatable : public base::SupportsUserData {
   public:
    SupportsUserDataAggregatable() = default;

    SupportsUserDataAggregatable(const SupportsUserDataAggregatable&) = delete;
    SupportsUserDataAggregatable& operator=(
        const SupportsUserDataAggregatable&) = delete;

    ~SupportsUserDataAggregatable() override {}
  };

  // The task runner that this class uses for its UI tasks.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Storage for user data to be accessed only on the DB sequence. May
  // be used e.g. for SyncableService subclasses that need to be owned
  // by this object. Is created on first call to |GetDBUserData()|.
  std::unique_ptr<SupportsUserDataAggregatable> user_data_;

  base::ObserverList<AutofillWebDataServiceObserverOnDBSequence>::Unchecked
      db_observer_list_;

  // WebDatabaseBackend allows direct access to DB.
  // TODO(caitkp): Make it so nobody but us needs direct DB access anymore.
  scoped_refptr<WebDatabaseBackend> web_database_backend_;

  base::RepeatingCallback<void(syncer::DataType)>
      on_autofill_changed_by_sync_callback_;
  base::RepeatingCallback<void(const AutofillProfileChange&)>
      on_autofill_profile_changed_cb_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
