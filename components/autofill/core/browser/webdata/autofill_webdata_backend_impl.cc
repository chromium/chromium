// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
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
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"

using base::Time;

namespace autofill {

namespace {
WebDatabase::State DoNothingAndCommit(WebDatabase* db) {
  return WebDatabase::COMMIT_NEEDED;
}
}  // namespace

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDatabaseBackend> web_database_backend,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
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
    return WebDatabase::COMMIT_NEEDED;
  }
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

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddAutofillProfile(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::ADD, profile.guid(),
                               &profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_,
                                  AutofillProfileDeepChange(
                                      AutofillProfileChange::ADD, profile)));
  }

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
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::UPDATE, profile.guid(),
                               &profile);
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_autofill_profile_changed_cb_,
                                  AutofillProfileDeepChange(
                                      AutofillProfileChange::UPDATE, profile)));
  }

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
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(
          guid, profile_source)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid,
                               profile.get());
  for (auto& db_observer : db_observer_list_)
    db_observer.AutofillProfileChanged(change);

  if (!on_autofill_profile_changed_cb_.is_null()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(on_autofill_profile_changed_cb_,
                       AutofillProfileDeepChange(AutofillProfileChange::REMOVE,
                                                 *profile.get())));
  }

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    AutofillProfile::Source profile_source,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles,
                                                          profile_source);
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
          autofill_entries))
    return WebDatabase::COMMIT_NOT_NEEDED;

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card,
    WebDatabase* db) {
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
    const CreditCard& credit_card,
    WebDatabase* db) {
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
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<CreditCard> card =
      AutofillTable::FromWebDatabase(db)->GetCreditCard(guid);
  if (!card) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::REMOVE, guid, card.get()));
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
    const std::u16string& full_number,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->UnmaskServerCreditCard(card,
                                                                 full_number))
    return WebDatabase::COMMIT_NEEDED;
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::MaskServerCreditCard(
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
  DCHECK_NE(CreditCard::LOCAL_CARD, card.record_type());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerCardMetadata(card))
    return WebDatabase::COMMIT_NOT_NEEDED;

  for (auto& db_observer : db_observer_list_) {
    db_observer.CreditCardChanged(
        CreditCardChange(CreditCardChange::UPDATE, card.server_id(), &card));
  }

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetIBANs(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<IBAN>> ibans;
  AutofillTable::FromWebDatabase(db)->GetIBANs(&ibans);

  return std::make_unique<WDResult<std::vector<std::unique_ptr<IBAN>>>>(
      AUTOFILL_IBANS_RESULT, std::move(ibans));
}

WebDatabase::State AutofillWebDataBackendImpl::AddIBAN(const IBAN& iban,
                                                       WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (!AutofillTable::FromWebDatabase(db)->AddIBAN(iban)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IBANChanged(IBANChange(IBANChange::ADD, iban.guid(), &iban));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateIBAN(const IBAN& iban,
                                                          WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  // It is currently valid to try to update a missing IBAN. We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<IBAN> original_iban =
      AutofillTable::FromWebDatabase(db)->GetIBAN(iban.guid());
  if (!original_iban)
    return WebDatabase::COMMIT_NOT_NEEDED;

  if (!AutofillTable::FromWebDatabase(db)->UpdateIBAN(iban)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IBANChanged(IBANChange(IBANChange::UPDATE, iban.guid(), &iban));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveIBAN(
    const std::string& guid,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<IBAN> iban =
      AutofillTable::FromWebDatabase(db)->GetIBAN(guid);
  if (!iban) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveIBAN(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.IBANChanged(IBANChange(IBANChange::REMOVE, guid, iban.get()));
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerAddressMetadata(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(AutofillProfile::SERVER_PROFILE, profile.record_type());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerAddressMetadata(
          profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (auto& db_observer : db_observer_list_) {
    db_observer.AutofillProfileChanged(AutofillProfileChange(
        AutofillProfileChange::UPDATE, profile.server_id(), &profile));
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddUpiId(
    const std::string& upi_id,
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  if (!AutofillTable::FromWebDatabase(db)->InsertUpiId(upi_id))
    return WebDatabase::COMMIT_NOT_NEEDED;
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAllUpiIds(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  std::vector<std::string> upi_ids =
      AutofillTable::FromWebDatabase(db)->GetAllUpiIds();
  return std::make_unique<WDResult<std::vector<std::string>>>(
      AUTOFILL_UPI_RESULT, std::move(upi_ids));
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
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  if (AutofillTable::FromWebDatabase(db)->RemoveAutofillDataModifiedBetween(
          delete_begin, delete_end, &profiles, &credit_cards)) {
    for (const std::unique_ptr<AutofillProfile>& profile : profiles) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.AutofillProfileChanged(AutofillProfileChange(
            AutofillProfileChange::REMOVE, profile->guid(), profile.get()));
      }
    }
    for (const std::unique_ptr<CreditCard>& credit_card : credit_cards) {
      for (auto& db_observer : db_observer_list_) {
        db_observer.CreditCardChanged(CreditCardChange(
            CreditCardChange::REMOVE, credit_card->guid(), credit_card.get()));
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

WebDatabase::State AutofillWebDataBackendImpl::RemoveOrphanAutofillTableRows(
    WebDatabase* db) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  if (AutofillTable::FromWebDatabase(db)->RemoveOrphanAutofillTableRows()) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

}  // namespace autofill
