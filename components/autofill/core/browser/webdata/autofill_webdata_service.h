// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/base/data_type.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

class WebDatabaseService;

namespace base {
class SequencedTaskRunner;
}

namespace autofill {

class AutocompleteEntry;
class AutofillWebDataBackend;
class AutofillWebDataBackendImpl;
class AutofillWebDataServiceObserverOnDBSequence;
class AutofillWebDataServiceObserverOnUISequence;
class CreditCard;
class Iban;

// API for Autofill web data.
class AutofillWebDataService : public WebDataServiceBase {
 public:
  // Runs db tasks on the wdbs db task runner.
  AutofillWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  AutofillWebDataService(const AutofillWebDataService&) = delete;
  AutofillWebDataService& operator=(const AutofillWebDataService&) = delete;

  // WebDataServiceBase implementation.
  void ShutdownOnUISequence() override;

  // Schedules a task to add form fields to the web database.
  virtual void AddFormFields(const std::vector<FormFieldData>& fields);

  // Initiates the request for a vector of values which have been entered in
  // form input fields named |name|. The method OnWebDataServiceRequestDone of
  // |consumer| gets called back when the request is finished, with the vector
  // included in the argument |result|.
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const std::u16string& name,
      const std::u16string& prefix,
      int limit,
      WebDataServiceRequestCallback consumer);

  // Removes form elements recorded for Autocomplete from the database.
  void RemoveFormElementsAddedBetween(base::Time delete_begin,
                                      base::Time delete_end);
  void RemoveFormValueForElementName(const std::u16string& name,
                                     const std::u16string& value);

  // Schedules a task to add an Autofill profile to the web database.
  void AddAutofillProfile(
      const AutofillProfile& profile,
      base::OnceCallback<void(const AutofillProfileChange&)> on_success);

  // Schedules a task to update an Autofill profile in the web database.
  void UpdateAutofillProfile(
      const AutofillProfile& profile,
      base::OnceCallback<void(const AutofillProfileChange&)> on_success);

  // Schedules a task to remove an Autofill profile from the web database.
  // `guid` is the identifier of the profile to remove.
  // In practice `change_type` will either be `REMOVE` or `HIDE_IN_AUTOFILL`. It
  // will be used to determine what type of change (permanent remove or update)
  // should happen on the server. Both of them result in the entry being removed
  // from the local database.
  // Important: `HIDE_IN_AUTOFILL` should only be used for calls from the
  // deduplication logic for account profiles or for Home and Work.
  void RemoveAutofillProfile(
      const std::string& guid,
      AutofillProfileChange::Type change_type,
      base::OnceCallback<void(const AutofillProfileChange&)> on_success);

  // Initiates the request for Autofill profiles. The profiles are passed to the
  // `consumer` callback.
  WebDataServiceBase::Handle GetAutofillProfiles(
      WebDataServiceRequestCallback consumer);

  // See the identically named functions in EntityDataManager or EntityTable for
  // details.
  // `on_success` is called only if the operation has been completed.
  void AddOrUpdateEntityInstance(
      EntityInstance entity,
      base::OnceCallback<void(EntityInstanceChange)> on_success);
  void RemoveEntityInstance(
      EntityInstance entity,
      base::OnceCallback<void(EntityInstanceChange)> on_success);
  void RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                            base::Time delete_end);
  WebDataServiceBase::Handle GetEntityInstances(
      WebDataServiceRequestCallback consumer);

  // Updates the `EntityInstance::EntityMetadata` related to the given `entity`.
  void UpdateEntityMetadata(const EntityInstance& entity);

  // Retrieves LoyaltyCards from the database.
  WebDataServiceBase::Handle GetLoyaltyCards(
      WebDataServiceRequestCallback consumer);

  // Schedules a task to count the number of unique autofill values contained
  // in the time interval [|begin|, |end|). |begin| and |end| can be null
  // to indicate no time limitation.
  WebDataServiceBase::Handle GetCountOfValuesContainedBetween(
      base::Time begin,
      base::Time end,
      WebDataServiceRequestCallback consumer);

  // Schedules a task to update autocomplete entries in the web database.
  void UpdateAutocompleteEntries(
      const std::vector<AutocompleteEntry>& autocomplete_entries);

  // Schedules a task to add a local IBAN to the web database.
  void AddLocalIban(const Iban& iban);

  // Initiates the request for local/server IBANs. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the IBAN included in the argument |result|. The consumer
  // owns the IBAN.
  WebDataServiceBase::Handle GetLocalIbans(
      WebDataServiceRequestCallback consumer);
  WebDataServiceBase::Handle GetServerIbans(
      WebDataServiceRequestCallback consumer);

  // Schedules a task to update a local IBAN in the web database.
  void UpdateLocalIban(const Iban& iban);

  // Schedules a task to remove an existing local IBAN from the web database.
  // `guid` is the identifier of the IBAN to remove.
  void RemoveLocalIban(const std::string& guid);

  // Updates the metadata for a server IBAN.
  void UpdateServerIbanMetadata(const Iban& iban);

  // Schedules a task to add credit card to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Schedules a task to update credit card in the web database.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Schedules a task to update a local CVC in the web database.
  void UpdateLocalCvc(const std::string& guid, const std::u16string& cvc);

  // Schedules a task to remove a credit card from the web database.
  // |guid| is identifier of the credit card to remove.
  void RemoveCreditCard(const std::string& guid);

  // Methods to schedule a task to add, update, remove, clear server cvc in the
  // web database.
  void AddServerCvc(int64_t instrument_id, const std::u16string& cvc);
  void UpdateServerCvc(int64_t instrument_id, const std::u16string& cvc);
  void RemoveServerCvc(int64_t instrument_id);
  void ClearServerCvcs();

  // Method to clear all the local CVCs from the web database.
  void ClearLocalCvcs();

  // Method to clear all local CVCs created before mid-May 2025. For more
  // information, see crbug.com/411681430.
  void ClearLocalCvcsUpToMay2025();

#if BUILDFLAG(IS_IOS)
  // Method to clean up for crbug.com/445879524.
  void CleanupForCrbug445879524();
#endif  // BUILDFLAG(IS_IOS)

  // Initiates the request for local/server credit cards.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the credit cards included in the argument |result|.  The
  // consumer owns the credit cards.
  WebDataServiceBase::Handle GetCreditCards(
      WebDataServiceRequestCallback consumer);
  WebDataServiceBase::Handle GetServerCreditCards(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for Payments customer data.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the customer data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetPaymentsCustomerData(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for server credit card cloud token data. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the cloud token data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetCreditCardCloudTokenData(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for autofill offer data. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the offer data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetAutofillOffers(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for virtual card usage data. The method
  // OnWebDataServiceRequestDone() of `consumer` gets called when the request is
  // finished, with the virtual card usage data included in the argument
  // `result`. The consumer owns the data.
  WebDataServiceBase::Handle GetVirtualCardUsageData(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for credit card benefits. The method
  // OnWebDataServiceRequestDone() of `consumer` gets called when the request is
  // finished, with the credit card benefits included in the argument `result`.
  // The consumer owns the data.
  WebDataServiceBase::Handle GetCreditCardBenefits(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for masked bank accounts. The method
  // OnWebDataServiceRequestDone() of `consumer` gets called when the request is
  // finished, with the masked bank accounts included in the argument `result`.
  // The consumer owns the data.
  WebDataServiceBase::Handle GetMaskedBankAccounts(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for payment instruments. The method
  // OnWebDataServiceRequestDone() of `consumer` gets called when the request is
  // finished, with the payment instruments included in the argument `result`.
  // The consumer owns the data.
  WebDataServiceBase::Handle GetPaymentInstruments(
      WebDataServiceRequestCallback consumer);

  // Initiates the request for payment instrument creation options from local
  // storage. The method OnWebDataServiceRequestDone() of `consumer` gets called
  // when the request is finished, with the payment instrument creation options
  // included in the argument `result`. The consumer owns the data.
  WebDataServiceBase::Handle GetPaymentInstrumentCreationOptions(
      WebDataServiceRequestCallback consumer);

  // Clears all the credit card benefits from the database.
  void ClearAllCreditCardBenefits();

  void ClearAllServerData();

  // Updates the metadata for a server card (masked or not).
  void UpdateServerCardMetadata(const CreditCard& credit_card);

  void AddObserver(AutofillWebDataServiceObserverOnDBSequence* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnDBSequence* observer);

  void AddObserver(AutofillWebDataServiceObserverOnUISequence* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnUISequence* observer);

  // Returns a SupportsUserData object that may be used to store data accessible
  // from the DB sequence. Must be called only from the DB sequence.
  base::SupportsUserData& GetDBUserData();

  // Takes a callback which will be called on the DB sequence with a pointer to
  // an AutofillWebdataBackend. This backend can be used to access or update the
  // WebDatabase directly on the DB sequence.
  void GetAutofillBackend(
      base::OnceCallback<void(AutofillWebDataBackend*)> callback);

  // Returns a task runner that can be used to schedule tasks on the DB
  // sequence.
  scoped_refptr<base::SequencedTaskRunner> GetDBTaskRunner();

  // Triggers an Autocomplete retention policy run which will cleanup data that
  // hasn't been used since over the retention threshold.
  virtual WebDataServiceBase::Handle RemoveExpiredAutocompleteEntries(
      WebDataServiceRequestCallback consumer);

  // Schedules a task to add a server credit card to the web database.
  //
  // This is used for tests only. In production, server cards are set directly
  // by Chrome Sync code.
  void AddServerCreditCardForTesting(const CreditCard& credit_card);

 protected:
  ~AutofillWebDataService() override;

 private:
  // The task runner that this class uses for UI tasks.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  scoped_refptr<AutofillWebDataBackendImpl> autofill_backend_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
