// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"

using base::Bind;
using base::Time;

namespace autofill {

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback, ui_task_runner),
      ui_task_runner_(ui_task_runner),
      db_task_runner_(db_task_runner),
      autofill_backend_(nullptr) {
  base::Closure on_changed_callback =
      Bind(&AutofillWebDataService::NotifyAutofillMultipleChangedOnUISequence,
           weak_ptr_factory_.GetWeakPtr());
  base::Closure on_address_conversion_completed_callback =
      Bind(&AutofillWebDataService::
               NotifyAutofillAddressConversionCompletedOnUISequence,
           weak_ptr_factory_.GetWeakPtr());
  base::Callback<void(syncer::ModelType)> on_sync_started_callback =
      Bind(&AutofillWebDataService::NotifySyncStartedOnUISequence,
           weak_ptr_factory_.GetWeakPtr());
  autofill_backend_ = new AutofillWebDataBackendImpl(
      wdbs_->GetBackend(), ui_task_runner_, db_task_runner_,
      on_changed_callback, on_address_conversion_completed_callback,
      on_sync_started_callback);
}

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner)
    : WebDataServiceBase(nullptr,
                         WebDataServiceBase::ProfileErrorCallback(),
                         ui_task_runner),
      ui_task_runner_(ui_task_runner),
      db_task_runner_(db_task_runner),
      autofill_backend_(new AutofillWebDataBackendImpl(
          nullptr,
          ui_task_runner_,
          db_task_runner_,
          base::Closure(),
          base::Closure(),
          base::Callback<void(syncer::ModelType)>())) {}

void AutofillWebDataService::ShutdownOnUISequence() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  db_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&AutofillWebDataBackendImpl::ResetUserData, autofill_backend_));
  WebDataServiceBase::ShutdownOnUISequence();
}

void AutofillWebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::AddFormElements,
           autofill_backend_, fields));
}

WebDataServiceBase::Handle AutofillWebDataService::GetFormValuesForElementName(
    const base::string16& name, const base::string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetFormValuesForElementName,
           autofill_backend_, name, prefix, limit), consumer);
}

void AutofillWebDataService::RemoveFormElementsAddedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween,
           autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveFormValueForElementName,
           autofill_backend_, name, value));
}

void AutofillWebDataService::AddAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::AddAutofillProfile,
           autofill_backend_, profile));
}

void AutofillWebDataService::SetAutofillProfileChangedCallback(
    base::RepeatingCallback<void(const AutofillProfileDeepChange&)> change_cb) {
  autofill_backend_->SetAutofillProfileChangedCallback(std::move(change_cb));
}

void AutofillWebDataService::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::UpdateAutofillProfile,
           autofill_backend_, profile));
}

void AutofillWebDataService::RemoveAutofillProfile(
    const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveAutofillProfile,
           autofill_backend_, guid));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetAutofillProfiles, autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetServerProfiles, autofill_backend_),
      consumer);
}

void AutofillWebDataService::ConvertWalletAddressesAndUpdateWalletCards(
    const std::string& app_locale,
    const std::string& primary_account_email) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&AutofillWebDataBackendImpl::
                          ConvertWalletAddressesAndUpdateWalletCards,
                      autofill_backend_, app_locale, primary_account_email));
}

WebDataServiceBase::Handle
    AutofillWebDataService::GetCountOfValuesContainedBetween(
        const Time& begin, const Time& end, WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween,
           autofill_backend_, begin, end),
           consumer);
}

void AutofillWebDataService::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& autofill_entries) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        Bind(&AutofillWebDataBackendImpl::UpdateAutofillEntries,
                             autofill_backend_,
                             autofill_entries));
}

void AutofillWebDataService::AddCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::AddCreditCard,
           autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::UpdateCreditCard,
           autofill_backend_, credit_card));
}

void AutofillWebDataService::RemoveCreditCard(const std::string& guid) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveCreditCard,
           autofill_backend_, guid));
}

void AutofillWebDataService::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&AutofillWebDataBackendImpl::AddFullServerCreditCard,
                      autofill_backend_, credit_card));
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetCreditCards, autofill_backend_),
      consumer);
}

WebDataServiceBase::Handle AutofillWebDataService::GetServerCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetServerCreditCards,
           autofill_backend_),
      consumer);
}

void AutofillWebDataService::UnmaskServerCreditCard(
    const CreditCard& credit_card,
    const base::string16& full_number) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::UnmaskServerCreditCard,
           autofill_backend_, credit_card, full_number));
}

void AutofillWebDataService::MaskServerCreditCard(const std::string& id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::MaskServerCreditCard,
           autofill_backend_, id));
}

void AutofillWebDataService::AddVPA(const std::string& vpa_id) {
  wdbs_->ScheduleDBTask(FROM_HERE, Bind(&AutofillWebDataBackendImpl::AddVPA,
                                        autofill_backend_, vpa_id));
}

WebDataServiceBase::Handle AutofillWebDataService::GetPaymentsCustomerData(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::GetPaymentsCustomerData,
           autofill_backend_),
      consumer);
}

void AutofillWebDataService::ClearAllServerData() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::ClearAllServerData,
           autofill_backend_));
}

void AutofillWebDataService::ClearAllLocalData() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::ClearAllLocalData, autofill_backend_));
}

void AutofillWebDataService::UpdateServerCardMetadata(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&AutofillWebDataBackendImpl::UpdateServerCardMetadata,
                      autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateServerAddressMetadata(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&AutofillWebDataBackendImpl::UpdateServerAddressMetadata,
                      autofill_backend_, profile));
}

void AutofillWebDataService::RemoveAutofillDataModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween,
           autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveOriginURLsModifiedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween,
           autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveOrphanAutofillTableRows() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveOrphanAutofillTableRows,
           autofill_backend_));
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
    const base::Callback<void(AutofillWebDataBackend*)>& callback) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(callback, base::RetainedRef(autofill_backend_)));
}

base::SingleThreadTaskRunner* AutofillWebDataService::GetDBTaskRunner() {
  return db_task_runner_.get();
}

WebDataServiceBase::Handle
AutofillWebDataService::RemoveExpiredAutocompleteEntries(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      Bind(&AutofillWebDataBackendImpl::RemoveExpiredAutocompleteEntries,
           autofill_backend_),
      consumer);
}

AutofillWebDataService::~AutofillWebDataService() {
}

void AutofillWebDataService::NotifyAutofillMultipleChangedOnUISequence() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& ui_observer : ui_observer_list_)
    ui_observer.AutofillMultipleChangedBySync();
}

void AutofillWebDataService::
    NotifyAutofillAddressConversionCompletedOnUISequence() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& ui_observer : ui_observer_list_)
    ui_observer.AutofillAddressConversionCompleted();
}

void AutofillWebDataService::NotifySyncStartedOnUISequence(
    syncer::ModelType model_type) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& ui_observer : ui_observer_list_)
    ui_observer.SyncStarted(model_type);
}

}  // namespace autofill
