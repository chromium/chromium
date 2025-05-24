// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"

namespace autofill {

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebDataServiceBase(std::move(wdbs), ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      autofill_backend_(nullptr) {
  autofill_backend_ = new AutofillWebDataBackendImpl(
      wdbs_->GetBackend(), ui_task_runner_, wdbs_->GetDbSequence());
}

AutofillWebDataService::~AutofillWebDataService() = default;

void AutofillWebDataService::ShutdownOnUISequence() {
  autofill_backend_->ShutdownOnUISequence();
  WebDataServiceBase::ShutdownOnUISequence();
}

void AutofillWebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddFormElements,
                                autofill_backend_, fields));
}

WebDataServiceBase::Handle AutofillWebDataService::GetFormValuesForElementName(
    const std::u16string& name,
    const std::u16string& prefix,
    int limit,
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetFormValuesForElementName,
                     autofill_backend_, name, prefix, limit),
      std::move(consumer));
}

void AutofillWebDataService::RemoveFormElementsAddedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween,
          autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveFormValueForElementName(
    const std::u16string& name,
    const std::u16string& value) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::RemoveFormValueForElementName,
                     autofill_backend_, name, value));
}

void AutofillWebDataService::AddAutofillProfile(
    const AutofillProfile& profile,
    base::OnceCallback<void(const AutofillProfileChange&)> on_success) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::AddAutofillProfile,
                     autofill_backend_, profile, std::move(on_success)));
}

void AutofillWebDataService::UpdateAutofillProfile(
    const AutofillProfile& profile,
    base::OnceCallback<void(const AutofillProfileChange&)> on_success) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateAutofillProfile,
                     autofill_backend_, profile, std::move(on_success)));
}

void AutofillWebDataService::RemoveAutofillProfile(
    const std::string& guid,
    AutofillProfileChange::Type change_type,
    base::OnceCallback<void(const AutofillProfileChange&)> on_success) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::RemoveAutofillProfile,
                     autofill_backend_, guid, change_type,
                     std::move(on_success)));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillProfiles(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetAutofillProfiles,
                     autofill_backend_),
      std::move(consumer));
}

void AutofillWebDataService::AddOrUpdateEntityInstance(
    EntityInstance entity,
    base::OnceCallback<void(EntityInstanceChange)> on_success) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::AddOrUpdateEntityInstance,
                     autofill_backend_, std::move(entity),
                     std::move(on_success)));
}

void AutofillWebDataService::RemoveEntityInstance(
    base::Uuid guid,
    base::OnceCallback<void(EntityInstanceChange)> on_success) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::RemoveEntityInstance,
                     autofill_backend_, std::move(guid),
                     std::move(on_success)));
}

void AutofillWebDataService::RemoveEntityInstancesModifiedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveEntityInstancesModifiedBetween,
          autofill_backend_, delete_begin, delete_end));
}

WebDataServiceBase::Handle AutofillWebDataService::GetEntityInstances(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetEntityInstances,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetLoyaltyCards(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetLoyaltyCards,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle
AutofillWebDataService::GetCountOfValuesContainedBetween(
    base::Time begin,
    base::Time end,
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween,
          autofill_backend_, begin, end),
      std::move(consumer));
}

void AutofillWebDataService::UpdateAutocompleteEntries(
    const std::vector<AutocompleteEntry>& autocomplete_entries) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateAutocompleteEntries,
                     autofill_backend_, autocomplete_entries));
}

void AutofillWebDataService::AddCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddCreditCard,
                                autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::UpdateCreditCard,
                                autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateLocalCvc(const std::string& guid,
                                            const std::u16string& cvc) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::UpdateLocalCvc,
                                autofill_backend_, guid, cvc));
}

void AutofillWebDataService::RemoveCreditCard(const std::string& guid) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::RemoveCreditCard,
                                autofill_backend_, guid));
}

void AutofillWebDataService::AddLocalIban(const Iban& iban) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddLocalIban,
                                autofill_backend_, iban));
}

WebDataServiceBase::Handle AutofillWebDataService::GetLocalIbans(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetLocalIbans,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerIbans(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetServerIbans,
                     autofill_backend_),
      std::move(consumer));
}

void AutofillWebDataService::UpdateLocalIban(const Iban& iban) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::UpdateLocalIban,
                                autofill_backend_, iban));
}

void AutofillWebDataService::RemoveLocalIban(const std::string& guid) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::RemoveLocalIban,
                                autofill_backend_, guid));
}

void AutofillWebDataService::UpdateServerIbanMetadata(const Iban& iban) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateServerIbanMetadata,
                     autofill_backend_, iban));
}

void AutofillWebDataService::AddServerCvc(int64_t instrument_id,
                                          const std::u16string& cvc) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddServerCvc,
                                autofill_backend_, instrument_id, cvc));
}

void AutofillWebDataService::UpdateServerCvc(int64_t instrument_id,
                                             const std::u16string& cvc) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::UpdateServerCvc,
                                autofill_backend_, instrument_id, cvc));
}

void AutofillWebDataService::RemoveServerCvc(int64_t instrument_id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::RemoveServerCvc,
                                autofill_backend_, instrument_id));
}

void AutofillWebDataService::ClearServerCvcs() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::ClearServerCvcs,
                                autofill_backend_));
}

void AutofillWebDataService::ClearLocalCvcs() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::ClearLocalCvcs,
                                autofill_backend_));
}

void AutofillWebDataService::CleanupForCrbug411681430() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::CleanupForCrbug411681430,
                     autofill_backend_));
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCards(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetCreditCards,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerCreditCards(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetServerCreditCards,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetPaymentsCustomerData(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetPaymentsCustomerData,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCardCloudTokenData(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetCreditCardCloudTokenData,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillOffers(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetAutofillOffers,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetVirtualCardUsageData(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::GetAutofillVirtualCardUsageData,
          autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCardBenefits(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetCreditCardBenefits,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetMaskedBankAccounts(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetMaskedBankAccounts,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle AutofillWebDataService::GetPaymentInstruments(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetPaymentInstruments,
                     autofill_backend_),
      std::move(consumer));
}

WebDataServiceBase::Handle
AutofillWebDataService::GetPaymentInstrumentCreationOptions(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::GetPaymentInstrumentCreationOptions,
          autofill_backend_),
      std::move(consumer));
}

void AutofillWebDataService::ClearAllCreditCardBenefits() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::ClearAllCreditCardBenefits,
                     autofill_backend_));
}

void AutofillWebDataService::ClearAllServerData() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::ClearAllServerData,
                                autofill_backend_));
}

void AutofillWebDataService::UpdateServerCardMetadata(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateServerCardMetadata,
                     autofill_backend_, credit_card));
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  if (autofill_backend_)
    autofill_backend_->AddObserver(observer);
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  if (autofill_backend_)
    autofill_backend_->RemoveObserver(observer);
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnUISequence* observer) {
  if (autofill_backend_) {
    autofill_backend_->AddObserver(observer);
  }
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnUISequence* observer) {
  if (autofill_backend_) {
    autofill_backend_->RemoveObserver(observer);
  }
}

base::SupportsUserData* AutofillWebDataService::GetDBUserData() {
  return autofill_backend_->GetDBUserData();
}

void AutofillWebDataService::GetAutofillBackend(
    base::OnceCallback<void(AutofillWebDataBackend*)> callback) {
  wdbs_->GetDbSequence()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                base::RetainedRef(autofill_backend_)));
}

scoped_refptr<base::SequencedTaskRunner>
AutofillWebDataService::GetDBTaskRunner() {
  return wdbs_->GetDbSequence();
}

WebDataServiceBase::Handle
AutofillWebDataService::RemoveExpiredAutocompleteEntries(
    WebDataServiceRequestCallback consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveExpiredAutocompleteEntries,
          autofill_backend_),
      std::move(consumer));
}

void AutofillWebDataService::AddServerCreditCardForTesting(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::AddServerCreditCardForTesting,
                     autofill_backend_, credit_card));
}

}  // namespace autofill
