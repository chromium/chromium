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
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"

using base::Time;

namespace autofill {

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : WebDataServiceBase(std::move(wdbs), ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      db_task_runner_(std::move(db_task_runner)),
      autofill_backend_(nullptr) {
  base::RepeatingCallback<void(syncer::ModelType)>
      on_autofill_changed_by_sync_callback = base::BindRepeating(
          &AutofillWebDataService::NotifyOnAutofillChangedBySyncOnUISequence,
          weak_ptr_factory_.GetWeakPtr());
  autofill_backend_ = new AutofillWebDataBackendImpl(
      wdbs_->GetBackend(), ui_task_runner_, db_task_runner_,
      on_autofill_changed_by_sync_callback);
}

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : WebDataServiceBase(nullptr, ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      db_task_runner_(std::move(db_task_runner)),
      autofill_backend_(new AutofillWebDataBackendImpl(nullptr,
                                                       ui_task_runner_,
                                                       db_task_runner_,
                                                       base::NullCallback())) {}

void AutofillWebDataService::ShutdownOnUISequence() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  db_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&AutofillWebDataBackendImpl::ResetUserData, autofill_backend_));
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
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetFormValuesForElementName,
                     autofill_backend_, name, prefix, limit),
      consumer);
}

void AutofillWebDataService::RemoveFormElementsAddedBetween(
    const Time& delete_begin, const Time& delete_end) {
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
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddAutofillProfile,
                                autofill_backend_, profile));
}

void AutofillWebDataService::SetAutofillProfileChangedCallback(
    base::RepeatingCallback<void(const AutofillProfileChange&)> change_cb) {
  autofill_backend_->SetAutofillProfileChangedCallback(std::move(change_cb));
}

void AutofillWebDataService::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateAutofillProfile,
                     autofill_backend_, profile));
}

void AutofillWebDataService::RemoveAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::RemoveAutofillProfile,
                     autofill_backend_, guid, profile_source));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillProfiles(
    AutofillProfile::Source profile_source,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetAutofillProfiles,
                     autofill_backend_, profile_source),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetServerProfiles,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle
    AutofillWebDataService::GetCountOfValuesContainedBetween(
        const Time& begin, const Time& end, WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween,
          autofill_backend_, begin, end),
      consumer);
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

void AutofillWebDataService::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::AddFullServerCreditCard,
                     autofill_backend_, credit_card));
}

void AutofillWebDataService::AddLocalIban(const Iban& iban) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::AddLocalIban,
                                autofill_backend_, iban));
}

WebDataServiceBase::Handle AutofillWebDataService::GetLocalIbans(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetLocalIbans,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerIbans(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetServerIbans,
                     autofill_backend_),
      consumer);
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

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetCreditCards,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetServerCreditCards,
                     autofill_backend_),
      consumer);
}

void AutofillWebDataService::UnmaskServerCreditCard(
    const CreditCard& credit_card,
    const std::u16string& full_number) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UnmaskServerCreditCard,
                     autofill_backend_, credit_card, full_number));
}

void AutofillWebDataService::MaskServerCreditCard(const std::string& id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::MaskServerCreditCard,
                     autofill_backend_, id));
}

WebDataServiceBase::Handle AutofillWebDataService::GetPaymentsCustomerData(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetPaymentsCustomerData,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCardCloudTokenData(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetCreditCardCloudTokenData,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillOffers(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetAutofillOffers,
                     autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetVirtualCardUsageData(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::GetAutofillVirtualCardUsageData,
          autofill_backend_),
      consumer);
}

void AutofillWebDataService::ClearAllServerData() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::ClearAllServerData,
                                autofill_backend_));
}

void AutofillWebDataService::ClearAllLocalData() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce(&AutofillWebDataBackendImpl::ClearAllLocalData,
                                autofill_backend_));
}

void AutofillWebDataService::UpdateServerCardMetadata(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateServerCardMetadata,
                     autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateServerAddressMetadata(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateServerAddressMetadata,
                     autofill_backend_, profile));
}

void AutofillWebDataService::RemoveAutofillDataModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween,
          autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveOriginURLsModifiedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween,
          autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (autofill_backend_)
    autofill_backend_->AddObserver(observer);
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (autofill_backend_)
    autofill_backend_->RemoveObserver(observer);
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnUISequence* observer) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  ui_observer_list_.AddObserver(observer);
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnUISequence* observer) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  ui_observer_list_.RemoveObserver(observer);
}

base::SupportsUserData* AutofillWebDataService::GetDBUserData() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return autofill_backend_->GetDBUserData();
}

void AutofillWebDataService::GetAutofillBackend(
    base::OnceCallback<void(AutofillWebDataBackend*)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                base::RetainedRef(autofill_backend_)));
}

base::SequencedTaskRunner* AutofillWebDataService::GetDBTaskRunner() {
  return db_task_runner_.get();
}

WebDataServiceBase::Handle
AutofillWebDataService::RemoveExpiredAutocompleteEntries(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &AutofillWebDataBackendImpl::RemoveExpiredAutocompleteEntries,
          autofill_backend_),
      consumer);
}

AutofillWebDataService::~AutofillWebDataService() = default;

void AutofillWebDataService::NotifyOnAutofillChangedBySyncOnUISequence(
    syncer::ModelType model_type) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& ui_observer : ui_observer_list_)
    ui_observer.OnAutofillChangedBySync(model_type);
}

}  // namespace autofill
