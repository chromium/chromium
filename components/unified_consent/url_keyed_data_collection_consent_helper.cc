// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

#include "base/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/unified_consent/pref_names.h"

#include <map>
#include <set>

namespace unified_consent {

namespace {

class PrefBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper {
 public:
  explicit PrefBasedUrlKeyedDataCollectionConsentHelper(
      PrefService* pref_service);
  ~PrefBasedUrlKeyedDataCollectionConsentHelper() override = default;

  // UrlKeyedDataCollectionConsentHelper:
  bool IsEnabled() override;

 private:
  void OnPrefChanged();
  PrefService* pref_service_;  // weak (must outlive this)
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefBasedUrlKeyedDataCollectionConsentHelper);
};

class SyncBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper,
      syncer::SyncServiceObserver {
 public:
  SyncBasedUrlKeyedDataCollectionConsentHelper(
      syncer::SyncService* sync_service,
      std::set<syncer::ModelType> sync_data_types);
  ~SyncBasedUrlKeyedDataCollectionConsentHelper() override;

  // UrlKeyedDataCollectionConsentHelper:
  bool IsEnabled() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  void UpdateSyncDataTypeStates();

  syncer::SyncService* sync_service_;
  std::map<syncer::ModelType, syncer::UploadState> sync_data_type_states_;

  DISALLOW_COPY_AND_ASSIGN(SyncBasedUrlKeyedDataCollectionConsentHelper);
};

PrefBasedUrlKeyedDataCollectionConsentHelper::
    PrefBasedUrlKeyedDataCollectionConsentHelper(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(
          &PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged,
          base::Unretained(this)));
}

bool PrefBasedUrlKeyedDataCollectionConsentHelper::IsEnabled() {
  return pref_service_->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

void PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged() {
  FireOnStateChanged();
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    SyncBasedUrlKeyedDataCollectionConsentHelper(
        syncer::SyncService* sync_service,
        std::set<syncer::ModelType> sync_data_types)
    : sync_service_(sync_service) {
  DCHECK(!sync_data_types.empty());

  for (const auto& sync_data_type : sync_data_types) {
    sync_data_type_states_[sync_data_type] = syncer::UploadState::NOT_ACTIVE;
  }
  UpdateSyncDataTypeStates();

  if (sync_service_)
    sync_service_->AddObserver(this);
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    ~SyncBasedUrlKeyedDataCollectionConsentHelper() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

bool SyncBasedUrlKeyedDataCollectionConsentHelper::IsEnabled() {
  for (const auto& sync_data_type_states : sync_data_type_states_) {
    if (sync_data_type_states.second != syncer::UploadState::ACTIVE)
      return false;
  }
  return true;
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnStateChanged(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  bool enabled_before_state_updated = IsEnabled();
  UpdateSyncDataTypeStates();
  if (enabled_before_state_updated != IsEnabled())
    FireOnStateChanged();
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::UpdateSyncDataTypeStates() {
  for (auto iter = sync_data_type_states_.begin();
       iter != sync_data_type_states_.end(); ++iter) {
    iter->second = syncer::GetUploadToGoogleState(sync_service_, iter->first);
  }
}

}  // namespace

UrlKeyedDataCollectionConsentHelper::UrlKeyedDataCollectionConsentHelper() =
    default;
UrlKeyedDataCollectionConsentHelper::~UrlKeyedDataCollectionConsentHelper() =
    default;

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::NewAnonymizedDataCollectionConsentHelper(
    PrefService* pref_service,
    syncer::SyncService* sync_service) {
  return std::make_unique<PrefBasedUrlKeyedDataCollectionConsentHelper>(
      pref_service);
}

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::NewPersonalizedDataCollectionConsentHelper(
    syncer::SyncService* sync_service) {
  return std::make_unique<SyncBasedUrlKeyedDataCollectionConsentHelper>(
      sync_service, std::set<syncer::ModelType>(
                        {syncer::ModelType::HISTORY_DELETE_DIRECTIVES}));
}

void UrlKeyedDataCollectionConsentHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void UrlKeyedDataCollectionConsentHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UrlKeyedDataCollectionConsentHelper::FireOnStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnUrlKeyedDataCollectionConsentStateChanged(this);
}

}  // namespace unified_consent
