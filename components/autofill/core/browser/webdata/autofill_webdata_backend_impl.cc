// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"

using base::Time;

namespace autofill {

namespace {

// Do not modify the order or values of these elements as they are reported
// as metrics. This is represented as AutofillWebDataBackendImplOperationResult
// in enums.xml.
// The ID space is a bit stretched out to enable adding new failure testing
// inside functions where needed without destroying the order of elements.
enum class Result {
  kAddFormElements_Success = 0,
  kAddFormElements_Failure = 1,
  kRemoveFormElementsAddedBetween_Success = 10,
  kRemoveFormElementsAddedBetween_Failure = 11,
  kRemoveFormValueForElementName_Success = 20,
  kRemoveFormValueForElementName_Failure = 22,
  kAddAutofillProfile_Success = 30,
  kAddAutofillProfile_Failure = 31,
  kUpdateAutofillProfile_Success = 40,
  kUpdateAutofillProfile_ReadFailure = 41,
  kUpdateAutofillProfile_WriteFailure = 42,
  kRemoveAutofillProfile_Success = 50,
  kRemoveAutofillProfile_ReadFailure = 51,
  kRemoveAutofillProfile_WriteFailure = 52,
  kUpdateAutofillEntries_Success = 60,
  kUpdateAutofillEntries_Failure = 61,
  kAddCreditCard_Success = 70,
  kAddCreditCard_Failure = 71,
  kUpdateCreditCard_Success = 80,
  kUpdateCreditCard_ReadFailure = 81,
  kUpdateCreditCard_WriteFailure = 82,
  kRemoveCreditCard_Success = 90,
  kRemoveCreditCard_ReadFailure = 91,
  kRemoveCreditCard_WriteFailure = 92,
  kAddFullServerCreditCard_Success = 100,
  kAddFullServerCreditCard_Failure = 101,
  kUnmaskServerCreditCard_Success = 110,
  kUnmaskServerCreditCard_Failure = 111,
  kMaskServerCreditCard_Success = 120,
  kMaskServerCreditCard_Failure = 121,
  kUpdateServerCardMetadata_Success = 130,
  kUpdateServerCardMetadata_Failure = 131,
  kAddIban_Success = 140,
  kAddIban_Failure = 141,
  kUpdateIban_Success = 150,
  kUpdateIban_ReadFailure = 151,
  kUpdateIban_WriteFailure = 152,
  kRemoveIban_Success = 160,
  kRemoveIban_ReadFailure = 161,
  kRemoveIban_WriteFailure = 162,
  kUpdateServerAddressMetadata_Success = 170,
  kUpdateServerAddressMetadata_Failure = 171,
  kAddUpiId_Success = 180,
  kAddUpiId_Failure = 181,
  kClearAllServerData_Success = 190,
  kClearAllServerData_Failure = 191,
  kClearAllLocalData_Success = 200,
  kClearAllLocalData_Failure = 201,
  kRemoveAutofillDataModifiedBetween_Success = 210,
  kRemoveAutofillDataModifiedBetween_Failure = 211,
  kRemoveOriginURLsModifiedBetween_Success = 220,
  kRemoveOriginURLsModifiedBetween_Failure = 221,
  kAddServerCvc_Success = 230,
  kAddServerCvc_Failure = 231,
  kUpdateServerCvc_Success = 240,
  kUpdateServerCvc_Failure = 241,
  kRemoveServerCvc_Success = 250,
  kRemoveServerCvc_Failure = 251,
  kClearServerCvcs_Success = 260,
  kClearServerCvcs_Failure = 261,
  kMaxValue = kClearServerCvcs_Failure,
};

// Reports the success or failure of various operations on the database via UMA.
//
// Unit tests live in web_data_service_unittest.cc.
void ReportResult(Result result) {
  base::UmaHistogramEnumeration(
      "WebDatabase.AutofillWebDataBackendImpl.OperationResult", result);
}

WebDatabase::State DoNothingAndCommit(WebDatabase* db) {
  return WebDatabase::COMMIT_NEEDED;
}
}  // namespace

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDatabaseBackend> web_database_backend,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const base::RepeatingClosure& on_changed_callback,
    const base::RepeatingClosure& on_address_conversion_completed_callback,
    const base::RepeatingCallback<void(syncer::ModelType)>&
        on_sync_started_callback)
    : base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>(
          std::move(db_task_runner)),
      ui_task_runner_(ui_task_runner),
      web_database_backend_(web_database_backend),
      on_changed_callback_(on_changed_callback),
      on_address_conversion_completed_callback_(
          on_address_conversion_completed_callback),
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

void AutofillWebDataBackendImpl::SetAutofillProfileChangedCallback(
    base::RepeatingCallback<void(const AutofillProfileDeepChange&)> change_cb) {
  // The callback must be set only once, but it can be reset in tests.
  if (!on_autofill_profile_changed_cb_.is_null()) {
    CHECK_IS_TEST();
  }
  on_autofill_profile_changed_cb_ = std::move(change_cb);
}

WebDatabase* AutofillWebDataBackendImpl::GetDatabase() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  return web_database_backend_->database();
}

void AutofillWebDataBackendImpl::CommitChanges() {
  web_database_backend_->ExecuteWriteTask(base::BindOnce(&DoNothingAndCommit));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::RemoveExpiredAutocompleteEntries(WebDatabase* db) {
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
  }

  return std::make_unique<WDResult<size_t>>(AUTOFILL_CLEANUP_RESULT,
                                            changes.size());
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

  // UI sequence notification.
  ui_task_runner_->PostTask(FROM_HERE, on_changed_callback_);
}

void AutofillWebDataBackendImpl::NotifyOfAddressConversionCompleted() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // UI sequence notification.
  ui_task_runner_->PostTask(FROM_HERE,
                            on_address_conversion_completed_callback_);
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
    user_data_ = std::make_unique<SupportsUserDataAggregatable>();
  return user_data_.get();
}

void AutofillWebDataBackendImpl::ResetUserData() {
  user_data_.reset();
}

WebDatabase::State AutofillWebDataBackendImpl::AddFormElements(
    const std::vector<FormFieldData>& fields,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutofillChangeList changes;
  if (!AutofillTable::FromWebDatabase(db)->AddFormFieldValues(fields,
                                                              &changes)) {
    ReportResult(Result::kAddFormElements_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB sequence, and not the UI sequence.
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillEntriesChanged(changes);

  ReportResult(Result::kAddFormElements_Success);
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetFormValuesForElementName(
    const std::u16string& name,
    const std::u16string& prefix,
    int limit,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<AutofillEntry> entries;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &entries, limit);
  return std::make_unique<WDResult<std::vector<AutofillEntry>>>(
      AUTOFILL_VALUE_RESULT, entries);
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
    ReportResult(Result::kRemoveFormElementsAddedBetween_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kRemoveFormElementsAddedBetween_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormValueForElementName(
    const std::u16string& name,
    const std::u16string& value,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(
        AutofillChange(AutofillChange::REMOVE, AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    for (auto& db_observer : db_observer_list_)
      db_observer.AutofillEntriesChanged(changes);

    ReportResult(Result::kRemoveFormValueForElementName_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kRemoveFormValueForElementName_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddAutofillProfile(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    ReportResult(Result::kAddAutofillProfile_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::ADD, profile.guid(),
                               profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_,
                                  AutofillProfileDeepChange(
                                      AutofillProfileChange::ADD, profile)));
  }

  ReportResult(Result::kAddAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillProfile(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  std::unique_ptr<AutofillProfile> original_profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid(),
                                                             profile.source());
  if (!original_profile) {
    ReportResult(Result::kUpdateAutofillProfile_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfile(profile)) {
    ReportResult(Result::kUpdateAutofillProfile_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::UPDATE, profile.guid(),
                               profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_,
                                  AutofillProfileDeepChange(
                                      AutofillProfileChange::UPDATE, profile)));
  }

  ReportResult(Result::kUpdateAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<AutofillProfile> profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid,
                                                             profile_source);
  if (!profile) {
    ReportResult(Result::kRemoveAutofillProfile_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(
          guid, profile_source)) {
    ReportResult(Result::kRemoveAutofillProfile_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, *profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(on_autofill_profile_changed_cb_,
                       AutofillProfileDeepChange(AutofillProfileChange::REMOVE,
                                                 *profile.get())));
  }

  ReportResult(Result::kRemoveAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    AutofillProfile::Source profile_source,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(profile_source,
                                                          &profiles);
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

WebDatabase::State
AutofillWebDataBackendImpl::ConvertWalletAddressesAndUpdateWalletCards(
    const std::string& app_locale,
    const std::string& primary_account_email,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  return util::ConvertWalletAddressesAndUpdateWalletCards(
      app_locale, primary_account_email, this, db);
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween(
    const base::Time& begin,
    const base::Time& end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  int value =
      AutofillTable::FromWebDatabase(db)->GetCountOfValuesContainedBetween(
          begin, end);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<int>(AUTOFILL_VALUE_RESULT, value));
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& autofill_entries,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillEntries(
          autofill_entries)) {
    ReportResult(Result::kUpdateAutofillEntries_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  ReportResult(Result::kUpdateAutofillEntries_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    ReportResult(Result::kAddCreditCard_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::ADD, credit_card.guid(), credit_card));
  }
  ReportResult(Result::kAddCreditCard_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<CreditCard> original_credit_card =
      AutofillTable::FromWebDatabase(db)->GetCreditCard(credit_card.guid());
  if (!original_credit_card) {
    ReportResult(Result::kUpdateCreditCard_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->UpdateCreditCard(credit_card)) {
    ReportResult(Result::kUpdateCreditCard_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::UPDATE, credit_card.guid(), credit_card));
  }
  ReportResult(Result::kUpdateCreditCard_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveCreditCard(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<CreditCard> card =
      AutofillTable::FromWebDatabase(db)->GetCreditCard(guid);
  if (!card) {
    ReportResult(Result::kRemoveCreditCard_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    ReportResult(Result::kRemoveCreditCard_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::REMOVE, guid, *card));
  }
  ReportResult(Result::kRemoveCreditCard_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddFullServerCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddFullServerCreditCard(
          credit_card)) {
    ReportResult(Result::kAddFullServerCreditCard_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::ADD, credit_card.guid(), credit_card));
  }
  ReportResult(Result::kAddFullServerCreditCard_Success);
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
    const std::u16string& full_number,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->UnmaskServerCreditCard(card,
                                                                 full_number)) {
    ReportResult(Result::kUnmaskServerCreditCard_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kUnmaskServerCreditCard_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::MaskServerCreditCard(
    const std::string& id,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->MaskServerCreditCard(id)) {
    ReportResult(Result::kMaskServerCreditCard_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kMaskServerCreditCard_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCardMetadata(
    const CreditCard& card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_NE(CreditCard::RecordType::kLocalCard, card.record_type());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerCardMetadata(card)) {
    ReportResult(Result::kUpdateServerCardMetadata_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::UPDATE, card.server_id(), card));
  }

  ReportResult(Result::kUpdateServerCardMetadata_Success);
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetIbans(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<Iban>> ibans;
  AutofillTable::FromWebDatabase(db)->GetIbans(&ibans);

  return std::make_unique<WDResult<std::vector<std::unique_ptr<Iban>>>>(
      AUTOFILL_IBANS_RESULT, std::move(ibans));
}

WebDatabase::State AutofillWebDataBackendImpl::AddIban(const Iban& iban,
                                                       WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddIban(iban)) {
    ReportResult(Result::kAddIban_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::ADD, iban.guid(), iban));
  }
  ReportResult(Result::kAddIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateIban(const Iban& iban,
                                                          WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // It is currently valid to try to update a missing IBAN. We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<Iban> original_iban =
      AutofillTable::FromWebDatabase(db)->GetIban(iban.guid());
  if (!original_iban) {
    ReportResult(Result::kUpdateIban_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->UpdateIban(iban)) {
    ReportResult(Result::kUpdateIban_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::UPDATE, iban.guid(), iban));
  }
  ReportResult(Result::kUpdateIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveIban(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<Iban> iban =
      AutofillTable::FromWebDatabase(db)->GetIban(guid);
  if (!iban) {
    ReportResult(Result::kRemoveIban_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveIban(guid)) {
    ReportResult(Result::kRemoveIban_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::REMOVE, guid, *iban));
  }
  ReportResult(Result::kRemoveIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerAddressMetadata(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(AutofillProfile::SERVER_PROFILE, profile.record_type());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerAddressMetadata(
          profile)) {
    ReportResult(Result::kUpdateServerAddressMetadata_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.AutofillProfileChanged(AutofillProfileChange(
        AutofillProfileChange::UPDATE, profile.server_id(), profile));
  }

  ReportResult(Result::kUpdateServerAddressMetadata_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddServerCvc(
    int64_t instrument_id,
    const std::u16string& cvc,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->AddServerCvc(
          ServerCvc{instrument_id, cvc,
                    /*last_updated_timestamp=*/AutofillClock::Now()})) {
    ReportResult(Result::kAddServerCvc_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kAddServerCvc_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCvc(
    int64_t instrument_id,
    const std::u16string& cvc,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->UpdateServerCvc(
          ServerCvc{instrument_id, cvc,
                    /*last_updated_timestamp=*/AutofillClock::Now()})) {
    ReportResult(Result::kUpdateServerCvc_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kUpdateServerCvc_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveServerCvc(
    int64_t instrument_id,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->RemoveServerCvc(instrument_id)) {
    ReportResult(Result::kRemoveServerCvc_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kRemoveServerCvc_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearServerCvcs(
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->ClearServerCvcs()) {
    ReportResult(Result::kClearServerCvcs_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearServerCvcs_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddUpiId(
    const std::string& upi_id,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->InsertUpiId(upi_id)) {
    ReportResult(Result::kAddUpiId_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  ReportResult(Result::kAddUpiId_Success);
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

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCreditCardCloudTokenData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> cloud_token_data;
  AutofillTable::FromWebDatabase(db)->GetCreditCardCloudTokenData(
      &cloud_token_data);
  return std::make_unique<
      WDResult<std::vector<std::unique_ptr<CreditCardCloudTokenData>>>>(
      AUTOFILL_CLOUDTOKEN_RESULT, std::move(cloud_token_data));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillOffers(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  AutofillTable::FromWebDatabase(db)->GetAutofillOffers(&offers);
  return std::make_unique<
      WDResult<std::vector<std::unique_ptr<AutofillOfferData>>>>(
      AUTOFILL_OFFER_DATA, std::move(offers));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetAutofillVirtualCardUsageData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<VirtualCardUsageData>> virtual_card_usage_data;
  AutofillTable::FromWebDatabase(db)->GetAllVirtualCardUsageData(
      &virtual_card_usage_data);
  return std::make_unique<
      WDResult<std::vector<std::unique_ptr<VirtualCardUsageData>>>>(
      AUTOFILL_VIRTUAL_CARD_USAGE_DATA, std::move(virtual_card_usage_data));
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllServerData(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->ClearAllServerData()) {
    NotifyOfMultipleAutofillChanges();
    ReportResult(Result::kClearAllServerData_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearAllServerData_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllLocalData(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->ClearAllLocalData()) {
    NotifyOfMultipleAutofillChanges();
    ReportResult(Result::kClearAllLocalData_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearAllLocalData_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  if (AutofillTable::FromWebDatabase(db)->RemoveAutofillDataModifiedBetween(
          delete_begin, delete_end, &profiles, &credit_cards)) {
    for (const std::unique_ptr<AutofillProfile>& profile : profiles) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.AutofillProfileChanged(AutofillProfileChange(
            AutofillProfileChange::REMOVE, profile->guid(), *profile));
      }
    }
    for (const std::unique_ptr<CreditCard>& credit_card : credit_cards) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.CreditCardChanged(CreditCardChange(
            CreditCardChange::REMOVE, credit_card->guid(), *credit_card));
      }
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    ReportResult(Result::kRemoveAutofillDataModifiedBetween_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kRemoveAutofillDataModifiedBetween_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->RemoveOriginURLsModifiedBetween(
          delete_begin, delete_end)) {
    ReportResult(Result::kRemoveOriginURLsModifiedBetween_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  // Note: It is the caller's responsibility to post notifications for any
  // changes, e.g. by calling the Refresh() method of PersonalDataManager.
  ReportResult(Result::kRemoveOriginURLsModifiedBetween_Success);
  return WebDatabase::COMMIT_NEEDED;
}

}  // namespace autofill
