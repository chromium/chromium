// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

namespace base {
class SingleThreadTaskRunner;
}

class WebDatabaseBackend;

namespace autofill {

class AutofillProfile;
class AutofillWebDataServiceObserverOnDBSequence;
class CreditCard;

// Backend implentation for the AutofillWebDataService. This class runs on the
// DB sequence, as it handles reads and writes to the WebDatabase, and functions
// in it should only be called from that sequence. Most functions here are just
// the implementations of the corresponding functions in the Autofill
// WebDataService.
// This class is destroyed on the DB sequence.
class AutofillWebDataBackendImpl
    : public base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>,
      public AutofillWebDataBackend {
 public:
  // |web_database_backend| is used to access the WebDatabase directly for
  // Sync-related operations. |ui_task_runner| and |db_task_runner| are the task
  // runners that this class uses for UI and DB tasks respectively.
  // |on_changed_callback| is a closure which can be used to notify the UI
  // sequence of changes initiated by Sync (this callback may be called multiple
  // times).
  AutofillWebDataBackendImpl(
      scoped_refptr<WebDatabaseBackend> web_database_backend,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
      const base::Closure& on_changed_callback,
      const base::Closure& on_address_conversion_completed_callback,
      const base::Callback<void(syncer::ModelType)>& on_sync_started_callback);

  void SetAutofillProfileChangedCallback(
      base::RepeatingCallback<void(const AutofillProfileDeepChange&)>
          change_cb);

  // AutofillWebDataBackend implementation.
  void AddObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) override;
  void RemoveObserver(
      AutofillWebDataServiceObserverOnDBSequence* observer) override;
  WebDatabase* GetDatabase() override;
  void NotifyOfAutofillProfileChanged(
      const AutofillProfileChange& change) override;
  void NotifyOfCreditCardChanged(const CreditCardChange& change) override;
  void NotifyOfMultipleAutofillChanges() override;
  void NotifyOfAddressConversionCompleted() override;
  void NotifyThatSyncHasStarted(syncer::ModelType model_type) override;
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
      const base::string16& name,
      const base::string16& prefix,
      int limit,
      WebDatabase* db);

  // Function to remove expired Autocomplete entries, which deletes them from
  // the Sqlite table, unlinks them from Sync and cleans up the metadata.
  // Returns the number of entries cleaned-up.
  std::unique_ptr<WDTypedResult> RemoveExpiredAutocompleteEntries(
      WebDatabase* db);

  // Removes form elements recorded for Autocomplete from the database.
  WebDatabase::State RemoveFormElementsAddedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

  // Removes the Form-value |value| which has been entered in form input fields
  // named |name| from the database.
  WebDatabase::State RemoveFormValueForElementName(const base::string16& name,
                                                   const base::string16& value,
                                                   WebDatabase* db);

  // Adds an Autofill profile to the web database. Valid only for local
  // profiles.
  WebDatabase::State AddAutofillProfile(const AutofillProfile& profile,
                                        WebDatabase* db);

  // Updates an Autofill profile in the web database. Valid only for local
  // profiles.
  WebDatabase::State UpdateAutofillProfile(const AutofillProfile& profile,
                                           WebDatabase* db);

  // Removes an Autofill profile from the web database. Valid only for local
  // profiles.
  WebDatabase::State RemoveAutofillProfile(const std::string& guid,
                                           WebDatabase* db);

  // Returns the local/server Autofill profiles from the web database.
  std::unique_ptr<WDTypedResult> GetAutofillProfiles(WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetServerProfiles(WebDatabase* db);

  // Converts server profiles to local profiles, comparing profiles using
  // |app_locale| and filling in |primary_account_email| into newly converted
  // profiles. The task only converts profiles that have not been converted
  // before.
  WebDatabase::State ConvertWalletAddressesAndUpdateWalletCards(
      const std::string& app_locale,
      const std::string& primary_account_email,
      WebDatabase* db);

  // Returns the number of values such that all for autofill entries with that
  // value, the interval between creation date and last usage is entirely
  // contained between [|begin|, |end|).
  std::unique_ptr<WDTypedResult> GetCountOfValuesContainedBetween(
      const base::Time& begin,
      const base::Time& end,
      WebDatabase* db);

  // Updates Autofill entries in the web database.
  WebDatabase::State UpdateAutofillEntries(
      const std::vector<autofill::AutofillEntry>& autofill_entries,
      WebDatabase* db);

  // Adds a credit card to the web database. Valid only for local cards.
  WebDatabase::State AddCreditCard(const CreditCard& credit_card,
                                   WebDatabase* db);

  // Updates a credit card in the web database. Valid only for local cards.
  WebDatabase::State UpdateCreditCard(const CreditCard& credit_card,
                                      WebDatabase* db);

  // Removes a credit card from the web database. Valid only for local cards.
  WebDatabase::State RemoveCreditCard(const std::string& guid, WebDatabase* db);

  // Adds a full server credit card to the web database.
  WebDatabase::State AddFullServerCreditCard(const CreditCard& credit_card,
                                             WebDatabase* db);

  // Returns a vector of local/server credit cards from the web database.
  std::unique_ptr<WDTypedResult> GetCreditCards(WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetServerCreditCards(WebDatabase* db);

  // Server credit cards can be masked (only last 4 digits stored) or unmasked
  // (all data stored). These toggle between the two states.
  WebDatabase::State UnmaskServerCreditCard(const CreditCard& card,
                                            const base::string16& full_number,
                                            WebDatabase* db);
  WebDatabase::State MaskServerCreditCard(const std::string& id,
                                          WebDatabase* db);

  WebDatabase::State UpdateServerCardMetadata(const CreditCard& credit_card,
                                              WebDatabase* db);

  WebDatabase::State UpdateServerAddressMetadata(const AutofillProfile& profile,
                                                 WebDatabase* db);

  WebDatabase::State AddVPA(const std::string& vpa_id, WebDatabase* db);

  // Returns the PaymentsCustomerData from the database.
  std::unique_ptr<WDTypedResult> GetPaymentsCustomerData(WebDatabase* db);

  WebDatabase::State ClearAllServerData(WebDatabase* db);
  WebDatabase::State ClearAllLocalData(WebDatabase* db);

  // Removes Autofill records from the database. Valid only for local
  // cards/profiles.
  WebDatabase::State RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

  // Removes origin URLs associated with Autofill profiles and credit cards
  // from the database. Valid only for local cards/profiles.
  WebDatabase::State RemoveOriginURLsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

  // Removes the orphan rows in the autofill_profile_names,
  // autofill_profile_emails and autofill_profile_phones tables.
  WebDatabase::State RemoveOrphanAutofillTableRows(WebDatabase* db);

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
    SupportsUserDataAggregatable() {}
    ~SupportsUserDataAggregatable() override {}

   private:
    DISALLOW_COPY_AND_ASSIGN(SupportsUserDataAggregatable);
  };

  // The task runner that this class uses for its UI tasks.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Storage for user data to be accessed only on the DB sequence. May
  // be used e.g. for SyncableService subclasses that need to be owned
  // by this object. Is created on first call to |GetDBUserData()|.
  std::unique_ptr<SupportsUserDataAggregatable> user_data_;

  base::ObserverList<AutofillWebDataServiceObserverOnDBSequence>::Unchecked
      db_observer_list_;

  // WebDatabaseBackend allows direct access to DB.
  // TODO(caitkp): Make it so nobody but us needs direct DB access anymore.
  scoped_refptr<WebDatabaseBackend> web_database_backend_;

  base::Closure on_changed_callback_;
  base::Closure on_address_conversion_completed_callback_;
  base::Callback<void(syncer::ModelType)> on_sync_started_callback_;

  base::RepeatingCallback<void(const AutofillProfileDeepChange&)>
      on_autofill_profile_changed_cb_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataBackendImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
