// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
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
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner)
    : WebDataServiceBase(std::move(wdbs), ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      db_task_runner_(std::move(db_task_runner)),
      autofill_backend_(nullptr) {
  base::RepeatingClosure on_changed_callback = base::BindRepeating(
      &AutofillWebDataService::NotifyAutofillMultipleChangedOnUISequence,
      weak_ptr_factory_.GetWeakPtr());
  base::RepeatingClosure on_address_conversion_completed_callback =
      base::BindRepeating(
          &AutofillWebDataService::
              NotifyAutofillAddressConversionCompletedOnUISequence,
          weak_ptr_factory_.GetWeakPtr());
  base::RepeatingCallback<void(syncer::ModelType)> on_sync_started_callback =
      base::BindRepeating(
          &AutofillWebDataService::NotifySyncStartedOnUISequence,
          weak_ptr_factory_.GetWeakPtr());
  autofill_backend_ = new AutofillWebDataBackendImpl(
      wdbs_->GetBackend(), ui_task_runner_, db_task_runner_,
      on_changed_callback, on_address_conversion_completed_callback,
      on_sync_started_callback);
}

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner)
    : WebDataServiceBase(nullptr, ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      db_task_runner_(std::move(db_task_runner)),
      autofill_backend_(new AutofillWebDataBackendImpl(nullptr,
                                                       ui_task_runner_,
                                                       db_task_runner_,
                                                       base::NullCallback(),
                                                       base::NullCallback(),
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
    base::RepeatingCallback<void(const AutofillProfileDeepChange&)> change_cb) {
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

void AutofillWebDataService::ConvertWalletAddressesAndUpdateWalletCards(
    const std::string& app_locale,
    const std::string& primary_account_email) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::
                         ConvertWalletAddressesAndUpdateWalletCards,
                     autofill_backend_, app_locale, primary_account_email));
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

void AutofillWebDataService::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& autofill_entries) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::UpdateAutofillEntries,
                     autofill_backend_, autofill_entries));
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

void AutofillWebDataService::AddIBAN(const IBAN& iban) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::BindOnce(&AutofillWebDataBackendImpl::AddIBAN,
                                       autofill_backend_, iban));
}

WebDataServiceBase::Handle AutofillWebDataService::GetIBANs(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetIBANs, autofill_backend_),
      consumer);
}

void AutofillWebDataService::UpdateIBAN(const IBAN& iban) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::BindOnce(&AutofillWebDataBackendImpl::UpdateIBAN,
                                       autofill_backend_, iban));
}

void AutofillWebDataService::RemoveIBAN(const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::BindOnce(&AutofillWebDataBackendImpl::RemoveIBAN,
                                       autofill_backend_, guid));
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

void AutofillWebDataService::AddUpiId(const std::string& upi_id) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::BindOnce(&AutofillWebDataBackendImpl::AddUpiId,
                                       autofill_backend_, upi_id));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAllUpiIds(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::GetAllUpiIds,
                     autofill_backend_),
      consumer);
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

void AutofillWebDataService::RemoveOrphanAutofillTableRows() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&AutofillWebDataBackendImpl::RemoveOrphanAutofillTableRows,
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
    base::OnceCallback<void(AutofillWebDataBackend*)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                base::RetainedRef(autofill_backend_)));
}

base::SingleThreadTaskRunner* AutofillWebDataService::GetDBTaskRunner() {
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
