// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill {

PersonalDataManager::PersonalDataManager(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PrefService* pref_service,
    PrefService* local_state,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    syncer::SyncService* sync_service,
    StrikeDatabaseBase* strike_database,
    AutofillImageFetcherBase* image_fetcher,
    std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
    std::string app_locale,
    std::string variations_country_code)
    : pref_service_(pref_service),
      app_locale_(std::move(app_locale)),
      history_service_(history_service) {
  address_data_manager_ = std::make_unique<AddressDataManager>(
      profile_database, pref_service, local_state, sync_service,
      identity_manager, strike_database,
      GeoIpCountryCode(variations_country_code), app_locale_);
  payments_data_manager_ = std::make_unique<PaymentsDataManager>(
      profile_database, account_database, image_fetcher,
      std::move(shared_storage_handler), pref_service, sync_service,
      identity_manager, GeoIpCountryCode(std::move(variations_country_code)),
      app_locale_);
  address_data_manager_observation_.Observe(address_data_manager_.get());
  payments_data_manager_observation_.Observe(payments_data_manager_.get());

  // Listen for URL deletions from browsing history.
  if (history_service_) {
    history_service_observation_.Observe(history_service_.get());
  }

  // WebDataService may not be available in tests.
  if (!profile_database) {
    return;
  }

  Refresh();
  autofill_metrics::LogIsAutofillEnabledAtStartup(
      address_data_manager_->IsAutofillProfileEnabled() ||
      payments_data_manager_->IsAutofillPaymentMethodsEnabled());

  // Potentially import addresses and credit cards for testing.
  MaybeImportDataForManualTesting(weak_factory_.GetWeakPtr());
}

PersonalDataManager::~PersonalDataManager() = default;

void PersonalDataManager::Shutdown() {
  if (history_service_)
    history_service_observation_.Reset();
  history_service_ = nullptr;
  address_data_manager_->Shutdown();
  payments_data_manager_->Shutdown();
}

void PersonalDataManager::OnAddressDataChanged() {
  NotifyPersonalDataObserver();
}

void PersonalDataManager::OnPaymentsDataChanged() {
  NotifyPersonalDataObserver();
}

void PersonalDataManager::OnHistoryDeletions(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillCrowdsourcingManager::ClearUploadHistory(pref_service_);
  }
  address_data_manager_->OnHistoryDeletions(deletion_info);
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PersonalDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  address_data_manager_->SetSyncServiceForTest(sync_service);   // IN-TEST
  payments_data_manager_->SetSyncServiceForTest(sync_service);  // IN-TEST
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (!payments_data_manager_->RemoveByGUID(guid)) {
    address_data_manager_->RemoveProfile(guid);
  }
}

bool PersonalDataManager::IsDataLoaded() const {
  return address_data_manager_->has_initial_load_finished() &&
         payments_data_manager_->is_payments_data_loaded();
}

void PersonalDataManager::Refresh() {
  address_data_manager_->LoadProfiles();
  payments_data_manager_->Refresh();
}

void PersonalDataManager::NotifyPersonalDataObserver() {
  if (address_data_manager_->IsAwaitingPendingAddressChanges() ||
      payments_data_manager_->HasPendingPaymentQueries()) {
    return;
  }
  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnPersonalDataChanged();
  }
}

}  // namespace autofill
