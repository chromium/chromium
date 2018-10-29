// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"

using base::Bind;
using base::Time;

namespace autofill {

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDatabaseBackend> web_database_backend,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
    const base::Closure& on_changed_callback,
    const base::Callback<void(syncer::ModelType)>& on_sync_started_callback)
    : base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>(
          std::move(db_task_runner)),
      ui_task_runner_(ui_task_runner),
      web_database_backend_(web_database_backend),
      on_changed_callback_(on_changed_callback),
      on_sync_started_callback_(on_sync_started_callback) {}

void AutofillWebDataBackendImpl::AddObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  db_observer_list_.AddObserver(observer);
}

void AutofillWebDataBackendImpl::RemoveObserver(
    AutofillWebDataServiceObserverOnDBSequence* observer) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  db_observer_list_.RemoveObserver(observer);
}

AutofillWebDataBackendImpl::~AutofillWebDataBackendImpl() {
  DCHECK(!user_data_);  // Forgot to call ResetUserData?
}

WebDatabase* AutofillWebDataBackendImpl::GetDatabase() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  return web_database_backend_->database();
}

void AutofillWebDataBackendImpl::RemoveExpiredFormElements() {
  web_database_backend_->ExecuteWriteTask(
      Bind(&AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl,
           this));
}

void AutofillWebDataBackendImpl::NotifyOfAutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // DB sequence notification.
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);
}

void AutofillWebDataBackendImpl::NotifyOfCreditCardChanged(
    const CreditCardChange& change) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // DB sequence notification.
  for (auto& db_observer : db_observer_list_)
    db_observer.CreditCardChanged(change);
}

void AutofillWebDataBackendImpl::NotifyOfMultipleAutofillChanges() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // DB sequence notification.
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillMultipleChanged();

  // UI sequence notification.
  ui_task_runner_->PostTask(FROM_HERE, on_changed_callback_);
}

void AutofillWebDataBackendImpl::NotifyThatSyncHasStarted(
    syncer::ModelType model_type) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  if (on_sync_started_callback_.is_null())
    return;

  // UI sequence notification.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(on_sync_started_callback_, model_type));
}

base::SupportsUserData* AutofillWebDataBackendImpl::GetDBUserData() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!user_data_)
    user_data_.reset(new SupportsUserDataAggregatable());
  return user_data_.get();
}

void AutofillWebDataBackendImpl::ResetUserData() {
  user_data_.reset();
}

WebDatabase::State AutofillWebDataBackendImpl::AddFormElements(
    const std::vector<FormFieldData>& fields, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutofillChangeList changes;
  if (!AutofillTable::FromWebDatabase(db)->AddFormFieldValues(
        fields, &changes)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB sequence, and not the UI sequence.
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillEntriesChanged(changes);

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetFormValuesForElementName(
    const base::string16& name,
    const base::string16& prefix,
    int limit,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<base::string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<base::string16>>(AUTOFILL_VALUE_RESULT, values));
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElementsAddedBetween(
          delete_begin, delete_end, &changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB sequence, and not the UI sequence.
      for (auto& db_observer : db_observer_list_)
        db_observer.AutofillEntriesChanged(changes);
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(
        AutofillChange(AutofillChange::REMOVE, AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    for (auto& db_observer : db_observer_list_)
      db_observer.AutofillEntriesChanged(changes);

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  std::unique_ptr<AutofillProfile> original_profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid());
  if (!original_profile) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::UPDATE, profile.guid(), &profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveAutofillProfile(
    const std::string& guid, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<AutofillProfile> profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid);
  if (!profile) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, nullptr);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<std::unique_ptr<AutofillProfile>>>(
          AUTOFILL_PROFILES_RESULT, std::move(profiles)));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerProfiles(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  AutofillTable::FromWebDatabase(db)->GetServerProfiles(&profiles);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<std::unique_ptr<AutofillProfile>>>(
          AUTOFILL_PROFILES_RESULT, std::move(profiles)));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween(
    const base::Time& begin,
    const base::Time& end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  int value = AutofillTable::FromWebDatabase(db)
      ->GetCountOfValuesContainedBetween(begin, end);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<int>(AUTOFILL_VALUE_RESULT, value));
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& autofill_entries,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)
           ->UpdateAutofillEntries(autofill_entries))
    return WebDatabase::COMMIT_NOT_NEEDED;

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::ADD, credit_card.guid(), &credit_card));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<CreditCard> original_credit_card =
      AutofillTable::FromWebDatabase(db)->GetCreditCard(credit_card.guid());
  if (!original_credit_card)
    return WebDatabase::COMMIT_NOT_NEEDED;

  if (!AutofillTable::FromWebDatabase(db)->UpdateCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::UPDATE, credit_card.guid(), &credit_card));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveCreditCard(
    const std::string& guid, WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::REMOVE, guid, nullptr));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddFullServerCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddFullServerCreditCard(
          credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::ADD, credit_card.guid(), &credit_card));
  }
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetCreditCards(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<std::unique_ptr<CreditCard>>>(
          AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards)));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerCreditCards(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetServerCreditCards(&credit_cards);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<std::unique_ptr<CreditCard>>>(
          AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards)));
}

WebDatabase::State AutofillWebDataBackendImpl::UnmaskServerCreditCard(
    const CreditCard& card,
    const base::string16& full_number,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->UnmaskServerCreditCard(
          card, full_number))
    return WebDatabase::COMMIT_NEEDED;
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
    AutofillWebDataBackendImpl::MaskServerCreditCard(
        const std::string& id,
        WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->MaskServerCreditCard(id))
    return WebDatabase::COMMIT_NEEDED;
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCardMetadata(
    const CreditCard& card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerCardMetadata(card))
    return WebDatabase::COMMIT_NOT_NEEDED;

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerAddressMetadata(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerAddressMetadata(
          profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.AutofillProfileChanged(AutofillProfileChange(
        AutofillProfileChange::UPDATE, profile.guid(), &profile));
  }

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetPaymentsCustomerData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<PaymentsCustomerData> customer_data;
  AutofillTable::FromWebDatabase(db)->GetPaymentsCustomerData(&customer_data);
  return std::make_unique<WDResult<std::unique_ptr<PaymentsCustomerData>>>(
      AUTOFILL_CUSTOMERDATA_RESULT, std::move(customer_data));
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllServerData(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->ClearAllServerData()) {
    NotifyOfMultipleAutofillChanges();
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllLocalData(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->ClearAllLocalData()) {
    NotifyOfMultipleAutofillChanges();
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
    AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (AutofillTable::FromWebDatabase(db)->RemoveAutofillDataModifiedBetween(
          delete_begin,
          delete_end,
          &profile_guids,
          &credit_card_guids)) {
    for (const std::string& guid : profile_guids) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.AutofillProfileChanged(AutofillProfileChange(
            AutofillProfileChange::REMOVE, guid, nullptr));
      }
    }
    for (const std::string& guid : credit_card_guids) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.CreditCardChanged(
            CreditCardChange(CreditCardChange::REMOVE, guid, nullptr));
      }
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  if (!AutofillTable::FromWebDatabase(db)->RemoveOriginURLsModifiedBetween(
          delete_begin, delete_end, &profiles)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (const auto& profile : profiles) {
    AutofillProfileChange change(AutofillProfileChange::UPDATE, profile->guid(),
                                 profile.get());
    for (auto& db_observer : db_observer_list_)
      db_observer.AutofillProfileChanged(change);
  }
  // Note: It is the caller's responsibility to post notifications for any
  // changes, e.g. by calling the Refresh() method of PersonalDataManager.
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB sequence, and not the UI sequence.
      for (auto& db_observer : db_observer_list_)
        db_observer.AutofillEntriesChanged(changes);
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveOrphanAutofillTableRows(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->RemoveOrphanAutofillTableRows()) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

}  // namespace autofill
