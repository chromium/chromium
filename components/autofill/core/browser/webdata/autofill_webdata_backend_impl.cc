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
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/base/data_type.h"
#include "components/webdata/common/web_database_backend.h"

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
  kUpdateAutocompleteEntries_Success = 60,
  kUpdateAutocompleteEntries_Failure = 61,
  kAddCreditCard_Success = 70,
  kAddCreditCard_Failure = 71,
  kUpdateCreditCard_Success = 80,
  kUpdateCreditCard_ReadFailure = 81,
  kUpdateCreditCard_WriteFailure = 82,
  kRemoveCreditCard_Success = 90,
  kRemoveCreditCard_ReadFailure = 91,
  kRemoveCreditCard_WriteFailure = 92,
  kAddServerCreditCardForTesting_Success = 100,
  kAddServerCreditCardForTesting_Failure = 101,
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
  // Server addresses metadata updates (170, 171) are deprecated.
  kAddUpiId_Success = 180,
  kAddUpiId_Failure = 181,
  kClearAllServerData_Success = 190,
  kClearAllServerData_Failure = 191,
  // Clearing of local data (200, 201) is deprecated.
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
  kUpdateCreditCardCvc_Success = 270,
  kUpdateCreditCardCvc_Failure = 271,
  kClearLocalCvcs_Success = 272,
  kClearLocalCvcs_Failure = 273,
  kUpdateServerIbanMetadata_Success = 274,
  kUpdateServerIbanMetadata_Failure = 275,
  kClearAllCreditCardBenefits_Success = 276,
  kClearAllCreditCardBenefits_Failure = 277,
  kMaxValue = kClearAllCreditCardBenefits_Failure,
};

// Reports the success or failure of various operations on the database via UMA.
//
// Unit tests live in web_data_service_unittest.cc.
void ReportResult(Result result) {
  base::UmaHistogramEnumeration(
      "WebDatabase.AutofillWebDataBackendImpl.OperationResult", result);
}

}  // namespace

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDatabaseBackend> web_database_backend,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const base::RepeatingCallback<void(syncer::DataType)>&
        on_autofill_changed_by_sync_callback)
    : base::RefCountedDeleteOnSequence<AutofillWebDataBackendImpl>(
          std::move(db_task_runner)),
      ui_task_runner_(ui_task_runner),
      web_database_backend_(web_database_backend),
      on_autofill_changed_by_sync_callback_(
          on_autofill_changed_by_sync_callback) {}

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
    base::RepeatingCallback<void(const AutofillProfileChange&)> change_cb) {
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
  web_database_backend_->database()->CommitTransaction();
  web_database_backend_->database()->BeginTransaction();
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::RemoveExpiredAutocompleteEntries(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutocompleteChangeList changes;
  if (AutocompleteTable::FromWebDatabase(db)->RemoveExpiredFormElements(
          changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB sequence, and not the UI sequence.
      for (auto& db_observer : db_observer_list_)
        db_observer.AutocompleteEntriesChanged(changes);
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

void AutofillWebDataBackendImpl::NotifyOfIbanChanged(const IbanChange& change) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // DB sequence notification.
  for (AutofillWebDataServiceObserverOnDBSequence& db_observer :
       db_observer_list_) {
    db_observer.IbanChanged(change);
  }
}

void AutofillWebDataBackendImpl::NotifyOnAutofillChangedBySync(
    syncer::DataType data_type) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // UI sequence notification.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(on_autofill_changed_by_sync_callback_, data_type));
}

void AutofillWebDataBackendImpl::NotifyOnServerCvcChanged(
    const ServerCvcChange& change) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // DB sequence notification.
  for (AutofillWebDataServiceObserverOnDBSequence& db_observer :
       db_observer_list_) {
    db_observer.ServerCvcChanged(change);
  }
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
  AutocompleteChangeList changes;
  if (!AutocompleteTable::FromWebDatabase(db)->AddFormFieldValues(fields,
                                                                  &changes)) {
    ReportResult(Result::kAddFormElements_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB sequence, and not the UI sequence.
  for (auto& db_observer : db_observer_list_)
    db_observer.AutocompleteEntriesChanged(changes);

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
  std::vector<AutocompleteEntry> entries;
  AutocompleteTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, limit, entries);
  return std::make_unique<WDResult<std::vector<AutocompleteEntry>>>(
      AUTOFILL_VALUE_RESULT, entries);
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AutocompleteChangeList changes;
  if (AutocompleteTable::FromWebDatabase(db)->RemoveFormElementsAddedBetween(
          delete_begin, delete_end, changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB sequence, and not the UI sequence.
      for (auto& db_observer : db_observer_list_)
        db_observer.AutocompleteEntriesChanged(changes);
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

  if (AutocompleteTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutocompleteChangeList changes;
    changes.push_back(AutocompleteChange(AutocompleteChange::REMOVE,
                                         AutocompleteKey(name, value)));

    // Post the notifications including the list of affected keys.
    for (auto& db_observer : db_observer_list_)
      db_observer.AutocompleteEntriesChanged(changes);

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
  AddressAutofillTable* table = AddressAutofillTable::FromWebDatabase(db);
  if (!table->AddAutofillProfile(profile)) {
    ReportResult(Result::kAddAutofillProfile_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  // The `db_profile` is not guaranteed to be equivalent to `profile`, since the
  // database might perform operations like `FinalizeAfterImport()`. Notify
  // observers with `db_profile`.
  AutofillProfile db_profile = *table->GetAutofillProfile(profile.guid());
  AutofillProfileChange change(AutofillProfileChange::ADD, profile.guid(),
                               std::move(db_profile));
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_, change));
  }

  ReportResult(Result::kAddAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillProfile(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  AddressAutofillTable* table = AddressAutofillTable::FromWebDatabase(db);
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  std::optional<AutofillProfile> original_profile =
      table->GetAutofillProfile(profile.guid());
  if (!original_profile) {
    ReportResult(Result::kUpdateAutofillProfile_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  if (!table->UpdateAutofillProfile(profile)) {
    ReportResult(Result::kUpdateAutofillProfile_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  // The `db_profile` is not guaranteed to be equivalent to `profile`, since the
  // database might perform operations like `FinalizeAfterImport()`. Notify
  // observers with `db_profile`.
  AutofillProfile db_profile = *table->GetAutofillProfile(profile.guid());
  AutofillProfileChange change(AutofillProfileChange::UPDATE, profile.guid(),
                               std::move(db_profile));
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_, change));
  }

  ReportResult(Result::kUpdateAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveAutofillProfile(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::optional<AutofillProfile> profile =
      AddressAutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid);
  if (!profile) {
    ReportResult(Result::kRemoveAutofillProfile_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AddressAutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(guid)) {
    ReportResult(Result::kRemoveAutofillProfile_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  // TODO(crbug.com/40258814): The change event for removal operations shouldn't
  // need to include the deleted profile. The GUID should suffice.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, *profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_, change));
  }

  ReportResult(Result::kRemoveAutofillProfile_Success);
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<AutofillProfile> profiles;
  AddressAutofillTable::FromWebDatabase(db)->GetAutofillProfiles(
      DenseSet<AutofillProfile::RecordType>::all(), profiles);
  return std::make_unique<WDResult<std::vector<AutofillProfile>>>(
      AUTOFILL_PROFILES_RESULT, std::move(profiles));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween(base::Time begin,
                                                             base::Time end,
                                                             WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  int value =
      AutocompleteTable::FromWebDatabase(db)->GetCountOfValuesContainedBetween(
          begin, end);
  return std::make_unique<WDResult<int>>(AUTOFILL_VALUE_RESULT, value);
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutocompleteEntries(
    const std::vector<AutocompleteEntry>& autocomplete_entries,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutocompleteTable::FromWebDatabase(db)->UpdateAutocompleteEntries(
          autocomplete_entries)) {
    ReportResult(Result::kUpdateAutocompleteEntries_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  ReportResult(Result::kUpdateAutocompleteEntries_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!PaymentsAutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
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
      PaymentsAutofillTable::FromWebDatabase(db)->GetCreditCard(
          credit_card.guid());
  if (!original_credit_card) {
    ReportResult(Result::kUpdateCreditCard_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!PaymentsAutofillTable::FromWebDatabase(db)->UpdateCreditCard(
          credit_card)) {
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

WebDatabase::State AutofillWebDataBackendImpl::UpdateLocalCvc(
    const std::string& guid,
    const std::u16string& cvc,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (PaymentsAutofillTable::FromWebDatabase(db)->UpdateLocalCvc(guid, cvc)) {
    ReportResult(Result::kUpdateCreditCardCvc_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kUpdateCreditCardCvc_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveCreditCard(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<CreditCard> card =
      PaymentsAutofillTable::FromWebDatabase(db)->GetCreditCard(guid);
  if (!card) {
    ReportResult(Result::kRemoveCreditCard_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!PaymentsAutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
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

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetCreditCards(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  PaymentsAutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
      AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerCreditCards(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  PaymentsAutofillTable::FromWebDatabase(db)->GetServerCreditCards(
      credit_cards);
  return std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
      AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCardMetadata(
    const CreditCard& card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_NE(CreditCard::RecordType::kLocalCard, card.record_type());
  if (!PaymentsAutofillTable::FromWebDatabase(db)->UpdateServerCardMetadata(
          card)) {
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

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetLocalIbans(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<Iban>> ibans;
  PaymentsAutofillTable::FromWebDatabase(db)->GetLocalIbans(&ibans);

  return std::make_unique<WDResult<std::vector<std::unique_ptr<Iban>>>>(
      AUTOFILL_IBANS_RESULT, std::move(ibans));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerIbans(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<Iban>> ibans;
  PaymentsAutofillTable::FromWebDatabase(db)->GetServerIbans(ibans);
  return std::make_unique<WDResult<std::vector<std::unique_ptr<Iban>>>>(
      AUTOFILL_IBANS_RESULT, std::move(ibans));
}

WebDatabase::State AutofillWebDataBackendImpl::AddLocalIban(const Iban& iban,
                                                            WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!PaymentsAutofillTable::FromWebDatabase(db)->AddLocalIban(iban)) {
    ReportResult(Result::kAddIban_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::ADD, iban.guid(), iban));
  }
  ReportResult(Result::kAddIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateLocalIban(
    const Iban& iban,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // It is currently valid to try to update a missing IBAN. We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<Iban> original_iban =
      PaymentsAutofillTable::FromWebDatabase(db)->GetLocalIban(iban.guid());
  if (!original_iban) {
    ReportResult(Result::kUpdateIban_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!PaymentsAutofillTable::FromWebDatabase(db)->UpdateLocalIban(iban)) {
    ReportResult(Result::kUpdateIban_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::UPDATE, iban.guid(), iban));
  }
  ReportResult(Result::kUpdateIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveLocalIban(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<Iban> iban =
      PaymentsAutofillTable::FromWebDatabase(db)->GetLocalIban(guid);
  if (!iban) {
    ReportResult(Result::kRemoveIban_ReadFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!PaymentsAutofillTable::FromWebDatabase(db)->RemoveLocalIban(guid)) {
    ReportResult(Result::kRemoveIban_WriteFailure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(IbanChange(IbanChange::REMOVE, guid, *iban));
  }
  ReportResult(Result::kRemoveIban_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerIbanMetadata(
    const Iban& iban,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  CHECK_EQ(Iban::RecordType::kServerIban, iban.record_type());
  if (!PaymentsAutofillTable::FromWebDatabase(db)
           ->AddOrUpdateServerIbanMetadata(iban.GetMetadata())) {
    ReportResult(Result::kUpdateServerIbanMetadata_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IbanChanged(
        IbanChange(IbanChange::UPDATE, iban.instrument_id(), iban));
  }

  ReportResult(Result::kUpdateServerIbanMetadata_Success);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddServerCvc(
    int64_t instrument_id,
    const std::u16string& cvc,
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  const ServerCvc server_cvc{instrument_id, cvc,
                             /*last_updated_timestamp=*/AutofillClock::Now()};
  if (PaymentsAutofillTable::FromWebDatabase(db)->AddServerCvc(server_cvc)) {
    const ServerCvcChange change{ServerCvcChange::ADD, instrument_id,
                                 server_cvc};
    for (auto& db_observer : db_observer_list_) {
      // TODO(crbug.com/40929129): Add integration tests for Add, Remove and
      // Update for Wallet Credential data.
      db_observer.ServerCvcChanged(change);
    }
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
  const ServerCvc server_cvc{instrument_id, cvc,
                             /*last_updated_timestamp=*/AutofillClock::Now()};
  if (PaymentsAutofillTable::FromWebDatabase(db)->UpdateServerCvc(server_cvc)) {
    const ServerCvcChange change{ServerCvcChange::UPDATE, instrument_id,
                                 server_cvc};
    for (auto& db_observer : db_observer_list_) {
      db_observer.ServerCvcChanged(change);
    }
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
  if (PaymentsAutofillTable::FromWebDatabase(db)->RemoveServerCvc(
          instrument_id)) {
    // Remove doesn't require `ServerCvc` struct data, so an empty data is
    // passed to the ServerCvcChange
    const ServerCvcChange change{ServerCvcChange::REMOVE, instrument_id,
                                 ServerCvc{}};
    for (auto& db_observer : db_observer_list_) {
      db_observer.ServerCvcChanged(change);
    }
    ReportResult(Result::kRemoveServerCvc_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kRemoveServerCvc_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearServerCvcs(
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<ServerCvc>> server_cvc_list =
      PaymentsAutofillTable::FromWebDatabase(db)->GetAllServerCvcs();
  if (PaymentsAutofillTable::FromWebDatabase(db)->ClearServerCvcs()) {
    for (const std::unique_ptr<ServerCvc>& server_cvc_from_list :
         server_cvc_list) {
      // Remove doesn't require `ServerCvc` struct data, so an empty data is
      // passed to the ServerCvcChange
      const ServerCvcChange change{ServerCvcChange::REMOVE,
                                   server_cvc_from_list->instrument_id,
                                   ServerCvc{}};
      for (auto& db_observer : db_observer_list_) {
        db_observer.ServerCvcChanged(change);
      }
    }
    ReportResult(Result::kClearServerCvcs_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearServerCvcs_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearLocalCvcs(WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (PaymentsAutofillTable::FromWebDatabase(db)->ClearLocalCvcs()) {
    ReportResult(Result::kClearLocalCvcs_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearLocalCvcs_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetPaymentsCustomerData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<PaymentsCustomerData> customer_data;
  PaymentsAutofillTable::FromWebDatabase(db)->GetPaymentsCustomerData(
      customer_data);
  return std::make_unique<WDResult<std::unique_ptr<PaymentsCustomerData>>>(
      AUTOFILL_CUSTOMERDATA_RESULT, std::move(customer_data));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCreditCardCloudTokenData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> cloud_token_data;
  PaymentsAutofillTable::FromWebDatabase(db)->GetCreditCardCloudTokenData(
      cloud_token_data);
  return std::make_unique<
      WDResult<std::vector<std::unique_ptr<CreditCardCloudTokenData>>>>(
      AUTOFILL_CLOUDTOKEN_RESULT, std::move(cloud_token_data));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillOffers(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  PaymentsAutofillTable::FromWebDatabase(db)->GetAutofillOffers(&offers);
  return std::make_unique<
      WDResult<std::vector<std::unique_ptr<AutofillOfferData>>>>(
      AUTOFILL_OFFER_DATA, std::move(offers));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetAutofillVirtualCardUsageData(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<VirtualCardUsageData> virtual_card_usage_data;
  PaymentsAutofillTable::FromWebDatabase(db)->GetAllVirtualCardUsageData(
      virtual_card_usage_data);
  return std::make_unique<WDResult<std::vector<VirtualCardUsageData>>>(
      AUTOFILL_VIRTUAL_CARD_USAGE_DATA, std::move(virtual_card_usage_data));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCreditCardBenefits(WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<CreditCardBenefit> credit_card_benefits;
  PaymentsAutofillTable::FromWebDatabase(db)->GetAllCreditCardBenefits(
      credit_card_benefits);
  return std::make_unique<WDResult<std::vector<CreditCardBenefit>>>(
      CREDIT_CARD_BENEFIT_RESULT, std::move(credit_card_benefits));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetMaskedBankAccounts(WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<BankAccount> masked_bank_accounts;
  PaymentsAutofillTable::FromWebDatabase(db)->GetMaskedBankAccounts(
      masked_bank_accounts);
  return std::make_unique<WDResult<std::vector<BankAccount>>>(
      MASKED_BANK_ACCOUNTS_RESULT, std::move(masked_bank_accounts));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetPaymentInstruments(WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PaymentsAutofillTable::FromWebDatabase(db)->GetPaymentInstruments(
      payment_instruments);
  return std::make_unique<WDResult<std::vector<sync_pb::PaymentInstrument>>>(
      PAYMENT_INSTRUMENT_RESULT, std::move(payment_instruments));
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllServerData(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (PaymentsAutofillTable::FromWebDatabase(db)->ClearAllServerData()) {
    ReportResult(Result::kClearAllServerData_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearAllServerData_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllCreditCardBenefits(
    WebDatabase* db) {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (PaymentsAutofillTable::FromWebDatabase(db)
          ->ClearAllCreditCardBenefits()) {
    ReportResult(Result::kClearAllCreditCardBenefits_Success);
    return WebDatabase::COMMIT_NEEDED;
  }
  ReportResult(Result::kClearAllCreditCardBenefits_Failure);
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddServerCreditCardForTesting(
    const CreditCard& credit_card,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!PaymentsAutofillTable::FromWebDatabase(db)
           ->AddServerCreditCardForTesting(credit_card)) {
    ReportResult(Result::kAddServerCreditCardForTesting_Failure);
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(CreditCardChange(
        CreditCardChange::ADD, credit_card.guid(), credit_card));
  }
  ReportResult(Result::kAddServerCreditCardForTesting_Success);
  return WebDatabase::COMMIT_NEEDED;
}

}  // namespace autofill
