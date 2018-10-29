// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"

#include <stddef.h>

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"

namespace autofill {

namespace {

void* AutofillWalletMetadataSyncableServiceUserDataKey() {
  // Use the address of a static so that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

// Sets the common syncable |metadata| for the |local_data_model|.
void SetCommonMetadata(sync_pb::WalletMetadataSpecifics::Type type,
                       const std::string& server_id,
                       const AutofillDataModel& local_data_model,
                       sync_pb::WalletMetadataSpecifics* metadata) {
  metadata->set_type(type);
  metadata->set_id(server_id);
  metadata->set_use_count(local_data_model.use_count());
  metadata->set_use_date(local_data_model.use_date().ToInternalValue());
}

// Returns syncable metadata for the |local_profile|.
syncer::SyncData BuildSyncData(sync_pb::WalletMetadataSpecifics::Type type,
                               const std::string& server_id,
                               const AutofillProfile& local_profile) {
  sync_pb::EntitySpecifics entity;
  sync_pb::WalletMetadataSpecifics* metadata = entity.mutable_wallet_metadata();
  SetCommonMetadata(type, server_id, local_profile, metadata);
  metadata->set_address_has_converted(local_profile.has_converted());
  std::string sync_tag = "address-" + server_id;

  return syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity);
}

// Returns syncable metadata for the |local_card|.
syncer::SyncData BuildSyncData(sync_pb::WalletMetadataSpecifics::Type type,
                               const std::string& server_id,
                               const CreditCard& local_card) {
  sync_pb::EntitySpecifics entity;
  sync_pb::WalletMetadataSpecifics* metadata = entity.mutable_wallet_metadata();
  SetCommonMetadata(type, server_id, local_card, metadata);
  // The strings must be in valid UTF-8 to sync.
  std::string billing_address_id;
  base::Base64Encode(local_card.billing_address_id(), &billing_address_id);
  metadata->set_card_billing_address_id(billing_address_id);
  std::string sync_tag = "card-" + server_id;

  return syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity);
}

// If the metadata exists locally, undelete it on the sync server.
template <class DataType>
void UndeleteMetadataIfExisting(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& metadata_type,
    std::unordered_map<std::string, std::unique_ptr<DataType>>* locals,
    syncer::SyncChangeList* changes_to_sync) {
  const auto& it = locals->find(server_id);
  if (it != locals->end()) {
    std::unique_ptr<DataType> local_metadata = std::move(it->second);
    locals->erase(it);
    changes_to_sync->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        BuildSyncData(metadata_type, server_id, *local_metadata)));
  }
}

syncer::SyncDataList::iterator FindServerIdAndTypeInCache(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& type,
    syncer::SyncDataList* cache) {
  for (auto it = cache->begin(); it != cache->end(); ++it) {
    if (server_id == it->GetSpecifics().wallet_metadata().id() &&
        type == it->GetSpecifics().wallet_metadata().type()) {
      return it;
    }
  }

  return cache->end();
}

syncer::SyncDataList::iterator FindItemInCache(const syncer::SyncData& item,
                                               syncer::SyncDataList* cache) {
  return FindServerIdAndTypeInCache(
      item.GetSpecifics().wallet_metadata().id(),
      item.GetSpecifics().wallet_metadata().type(), cache);
}

void RemoveItemFromCache(const syncer::SyncData& item,
                         syncer::SyncDataList* cache) {
  auto it = FindItemInCache(item, cache);
  if (it != cache->end())
    cache->erase(it);
}

void AddOrUpdateItemInCache(const syncer::SyncData& item,
                            syncer::SyncDataList* cache) {
  auto it = FindItemInCache(item, cache);
  if (it != cache->end())
    *it = item;
  else
    cache->push_back(item);
}

void ApplyChangesToCache(const syncer::SyncChangeList& changes,
                         syncer::SyncDataList* cache) {
  for (const syncer::SyncChange& change : changes) {
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      // Intentional fall through.
      case syncer::SyncChange::ACTION_UPDATE:
        AddOrUpdateItemInCache(change.sync_data(), cache);
        break;

      case syncer::SyncChange::ACTION_DELETE:
        RemoveItemFromCache(change.sync_data(), cache);
        break;

      case syncer::SyncChange::ACTION_INVALID:
        NOTREACHED();
        break;
    }
  }
}

// Merges the metadata of the remote and local versions of the data model.
void MergeCommonMetadata(
    const sync_pb::WalletMetadataSpecifics& remote_metadata,
    AutofillDataModel* local_model,
    bool* is_remote_outdated,
    bool* is_local_modified) {
  size_t remote_use_count =
      base::checked_cast<size_t>(remote_metadata.use_count());
  base::Time remote_use_date =
      base::Time::FromInternalValue(remote_metadata.use_date());

  // If the two models have the same metadata, do nothing.
  if (local_model->use_count() == remote_use_count &&
      local_model->use_date() == remote_use_date) {
    return;
  }

  // Special case for local models with a use_count of one. This means the local
  // model was only created, never used. The remote model should always be
  // preferred.
  // This situation can happen for new Chromium instances where there is no data
  // yet on disk, making the use_date artifically high. Once the metadata sync
  // kicks in, we should use that value.
  if (local_model->use_count() == 1) {
    local_model->set_use_date(remote_use_date);
    local_model->set_use_count(remote_use_count);
    *is_local_modified = true;
  } else {
    // Otherwise, just keep the most recent use date and biggest use count.
    if (local_model->use_date() < remote_use_date) {
      local_model->set_use_date(remote_use_date);
      *is_local_modified = true;
    } else if (local_model->use_date() > remote_use_date) {
      *is_remote_outdated = true;
    }

    if (local_model->use_count() < remote_use_count) {
      local_model->set_use_count(remote_use_count);
      *is_local_modified = true;
    } else if (local_model->use_count() > remote_use_count) {
      *is_remote_outdated = true;
    }
  }
}

// Merges the metadata of the remote and local versions of the profile.
void MergeMetadata(const sync_pb::WalletMetadataSpecifics& remote_metadata,
                   AutofillProfile* local_profile,
                   bool* is_remote_outdated,
                   bool* is_local_modified) {
  // Merge the has_converted status.
  if (local_profile->has_converted() !=
      remote_metadata.address_has_converted()) {
    if (!local_profile->has_converted()) {
      local_profile->set_has_converted(true);
      *is_local_modified = true;
    } else {
      *is_remote_outdated = true;
    }
  }

  // Merge the use_count and use_date.
  MergeCommonMetadata(remote_metadata, local_profile, is_remote_outdated,
                      is_local_modified);
}

// Whether the |current_billing_address_id| is considered outdated compared to
// the |proposed_billing_address_id|.
bool IsBillingAddressOutdated(const std::string& current_billing_address_id,
                              const std::string& proposed_billing_address_id) {
  DCHECK(current_billing_address_id != proposed_billing_address_id);

  // If the current billing address is empty, or if the current one refers to a
  // server address and the proposed one refers to a local address, the current
  // billing address is considered outdated.
  return current_billing_address_id.empty() ||
         (current_billing_address_id.size() != kLocalGuidSize &&
          proposed_billing_address_id.size() == kLocalGuidSize);
}

// Merges the metadata of the remote and local versions of the credit card.
void MergeMetadata(const sync_pb::WalletMetadataSpecifics& remote_metadata,
                   CreditCard* local_card,
                   bool* is_remote_outdated,
                   bool* is_local_modified) {
  // Merge the billing_address_id. Do this before updating the use_count
  // because it may be used to determine what id to keep.
  std::string remote_billing_address_id;
  base::Base64Decode(remote_metadata.card_billing_address_id(),
                     &remote_billing_address_id);

  if (local_card->billing_address_id() != remote_billing_address_id) {
    if (IsBillingAddressOutdated(local_card->billing_address_id(),
                                 remote_billing_address_id)) {
      local_card->set_billing_address_id(remote_billing_address_id);
      *is_local_modified = true;
    } else if (IsBillingAddressOutdated(remote_billing_address_id,
                                        local_card->billing_address_id())) {
      *is_remote_outdated = true;
    } else {
      // The cards have a different non-empty billing address id and both refer
      // to the same type of address. Keep the billing address id of the most
      // recently used card. If both have the same timestamp, the remote version
      // should be kept in order to stabilize the values.
      base::Time remote_use_date =
          base::Time::FromInternalValue(remote_metadata.use_date());
      if (local_card->use_date() <= remote_use_date) {
        local_card->set_billing_address_id(remote_billing_address_id);
        *is_local_modified = true;
      } else {
        *is_remote_outdated = true;
      }
    }
  }

  // Merge the use_count and use_date.
  MergeCommonMetadata(remote_metadata, local_card, is_remote_outdated,
                      is_local_modified);
}

// Merges |remote| metadata into a collection of metadata |locals|. Returns true
// if the corresponding local metadata was found.
//
// Stores an "update" in |changes_to_sync| if |remote| corresponds to an item in
// |locals| that has higher use count and later use date.
template <class DataType>
bool MergeRemote(
    const syncer::SyncData& remote,
    const base::Callback<bool(const DataType&)>& updater,
    std::unordered_map<std::string, std::unique_ptr<DataType>>* locals,
    syncer::SyncChangeList* changes_to_sync) {
  DCHECK(locals);
  DCHECK(changes_to_sync);

  const sync_pb::WalletMetadataSpecifics& remote_metadata =
      remote.GetSpecifics().wallet_metadata();
  auto it = locals->find(remote_metadata.id());
  if (it == locals->end())
    return false;

  std::unique_ptr<DataType> local_metadata = std::move(it->second);
  locals->erase(it);

  bool is_local_modified = false;
  bool is_remote_outdated = false;
  MergeMetadata(remote_metadata, local_metadata.get(), &is_remote_outdated,
                &is_local_modified);

  if (is_remote_outdated) {
    changes_to_sync->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        BuildSyncData(remote_metadata.type(), remote_metadata.id(),
                      *local_metadata)));
  }

  if (is_local_modified)
    updater.Run(*local_metadata);

  return true;
}

template <typename DataType>
std::string GetServerId(const DataType& data) {
  std::string server_id;
  base::Base64Encode(data.server_id(), &server_id);
  return server_id;
}

}  // namespace

AutofillWalletMetadataSyncableService::
    ~AutofillWalletMetadataSyncableService() {}

void AutofillWalletMetadataSyncableService::OnWalletDataTrackingStateChanged(
    bool is_tracking) {
  DCHECK_NE(track_wallet_data_, is_tracking);
  track_wallet_data_ = is_tracking;
  if (is_tracking && sync_processor_) {
    MergeData(cache_);
  }
}

syncer::SyncMergeResult
AutofillWalletMetadataSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_);
  DCHECK(!sync_error_factory_);
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_ = std::move(sync_processor);
  sync_error_factory_ = std::move(sync_error_factory);

  cache_ = initial_sync_data;

  syncer::SyncMergeResult result(syncer::AUTOFILL_WALLET_METADATA);
  if (track_wallet_data_) {
    result = MergeData(initial_sync_data);
  }

  // Notify that sync has started. This callback does not currently take into
  // account whether we're actually tracking wallet data.
  if (web_data_backend_)
    web_data_backend_->NotifyThatSyncHasStarted(type);
  return result;
}

void AutofillWalletMetadataSyncableService::StopSyncing(
    syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_.reset();
  sync_error_factory_.reset();
  cache_.clear();
}

syncer::SyncDataList AutofillWalletMetadataSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  syncer::SyncDataList data_list;
  std::unordered_map<std::string, std::unique_ptr<AutofillProfile>> profiles;
  std::unordered_map<std::string, std::unique_ptr<CreditCard>> cards;
  if (GetLocalData(&profiles, &cards)) {
    for (const auto& it : profiles) {
      data_list.push_back(BuildSyncData(
          sync_pb::WalletMetadataSpecifics::ADDRESS, it.first, *it.second));
    }

    for (const auto& it : cards) {
      data_list.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                        it.first, *it.second));
    }
  }

  return data_list;
}

syncer::SyncError AutofillWalletMetadataSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& changes_from_sync) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ApplyChangesToCache(changes_from_sync, &cache_);

  // If we're not tracking wallet data, we can't rely on the local wallet
  // data being up-to-date, so we should not do any merging with local data.
  if (!track_wallet_data_) {
    return syncer::SyncError();
  }

  std::unordered_map<std::string, std::unique_ptr<AutofillProfile>> profiles;
  std::unordered_map<std::string, std::unique_ptr<CreditCard>> cards;
  GetLocalData(&profiles, &cards);

  // base::Unretained is used because the callbacks are invoked synchronously.
  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncChange& change : changes_from_sync) {
    const sync_pb::WalletMetadataSpecifics& remote_metadata =
        change.sync_data().GetSpecifics().wallet_metadata();
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      // Intentional fall through.
      case syncer::SyncChange::ACTION_UPDATE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS:
            MergeRemote(change.sync_data(), address_updater, &profiles,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::CARD:
            MergeRemote(change.sync_data(), card_updater, &cards,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      // Metadata should only be deleted when the underlying data is deleted.
      case syncer::SyncChange::ACTION_DELETE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS:
            UndeleteMetadataIfExisting(
                remote_metadata.id(), sync_pb::WalletMetadataSpecifics::ADDRESS,
                &profiles, &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::CARD:
            UndeleteMetadataIfExisting(remote_metadata.id(),
                                       sync_pb::WalletMetadataSpecifics::CARD,
                                       &cards, &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      case syncer::SyncChange::ACTION_INVALID:
        NOTREACHED();
        break;
    }
  }

  syncer::SyncError status;
  if (!changes_to_sync.empty())
    status = SendChangesToSyncServer(changes_to_sync);

  return status;
}

void AutofillWalletMetadataSyncableService::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!track_wallet_data_) {
    return;
  }

  if (sync_processor_ && change.data_model() &&
      change.data_model()->record_type() != AutofillProfile::LOCAL_PROFILE) {
    std::string server_id = GetServerId(*change.data_model());
    auto it = FindServerIdAndTypeInCache(
        server_id, sync_pb::WalletMetadataSpecifics::ADDRESS, &cache_);
    if (it == cache_.end())
      return;
    // Implicitly, we filter out ADD (not in cache) and REMOVE (!data_model()).
    DCHECK(change.type() == AutofillProfileChange::UPDATE);

    AutofillDataModelUpdated(
        server_id, sync_pb::WalletMetadataSpecifics::ADDRESS,
        it->GetSpecifics().wallet_metadata(), *change.data_model());
  }
}

void AutofillWalletMetadataSyncableService::CreditCardChanged(
    const CreditCardChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!track_wallet_data_) {
    return;
  }

  if (sync_processor_ && change.data_model() &&
      change.data_model()->record_type() != CreditCard::LOCAL_CARD) {
    std::string server_id = GetServerId(*change.data_model());
    auto it = FindServerIdAndTypeInCache(
        server_id, sync_pb::WalletMetadataSpecifics::CARD, &cache_);
    if (it == cache_.end())
      return;
    // Implicitly, we filter out ADD (not in cache) and REMOVE (!data_model()).
    DCHECK(change.type() == AutofillProfileChange::UPDATE);

    AutofillDataModelUpdated(server_id, sync_pb::WalletMetadataSpecifics::CARD,
                             it->GetSpecifics().wallet_metadata(),
                             *change.data_model());
  }
}

void AutofillWalletMetadataSyncableService::AutofillMultipleChanged() {
  if (sync_processor_ && track_wallet_data_)
    MergeData(cache_);
}

// static
void AutofillWalletMetadataSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* web_data_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      AutofillWalletMetadataSyncableServiceUserDataKey(),
      base::WrapUnique(new AutofillWalletMetadataSyncableService(
          web_data_backend, app_locale)));
}

// static
AutofillWalletMetadataSyncableService*
AutofillWalletMetadataSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletMetadataSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(
          AutofillWalletMetadataSyncableServiceUserDataKey()));
}

AutofillWalletMetadataSyncableService::AutofillWalletMetadataSyncableService(
    AutofillWebDataBackend* web_data_backend,
    const std::string& app_locale)
    : web_data_backend_(web_data_backend),
      scoped_observer_(this),
      track_wallet_data_(false),
      weak_ptr_factory_(this) {
  scoped_observer_.Add(web_data_backend_);
}

bool AutofillWalletMetadataSyncableService::GetLocalData(
    std::unordered_map<std::string, std::unique_ptr<AutofillProfile>>* profiles,
    std::unordered_map<std::string, std::unique_ptr<CreditCard>>* cards) const {
  std::vector<std::unique_ptr<AutofillProfile>> profile_list;
  bool success =
      AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
          ->GetServerProfiles(&profile_list);
  while (!profile_list.empty()) {
    auto server_id = GetServerId(*profile_list.front());
    (*profiles)[server_id] = std::move(profile_list.front());
    profile_list.erase(profile_list.begin());
  }

  std::vector<std::unique_ptr<CreditCard>> card_list;
  success &= AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
                 ->GetServerCreditCards(&card_list);
  while (!card_list.empty()) {
    auto server_id = GetServerId(*card_list.front());
    (*cards)[server_id] = std::move(card_list.front());
    card_list.erase(card_list.begin());
  }

  return success;
}

bool AutofillWalletMetadataSyncableService::UpdateAddressStats(
    const AutofillProfile& profile) {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
      ->UpdateServerAddressMetadata(profile);
}

bool AutofillWalletMetadataSyncableService::UpdateCardStats(
    const CreditCard& credit_card) {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
      ->UpdateServerCardMetadata(credit_card);
}

syncer::SyncError
AutofillWalletMetadataSyncableService::SendChangesToSyncServer(
    const syncer::SyncChangeList& changes_to_sync) {
  DCHECK(sync_processor_);
  ApplyChangesToCache(changes_to_sync, &cache_);
  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes_to_sync);
}

syncer::SyncMergeResult AutofillWalletMetadataSyncableService::MergeData(
    const syncer::SyncDataList& sync_data) {
  // If we're not tracking wallet data, we can't rely on the local wallet
  // data being up-to-date, so we should not do any merging with local data.
  DCHECK(track_wallet_data_);

  std::unordered_map<std::string, std::unique_ptr<AutofillProfile>> profiles;
  std::unordered_map<std::string, std::unique_ptr<CreditCard>> cards;
  GetLocalData(&profiles, &cards);

  syncer::SyncMergeResult result(syncer::AUTOFILL_WALLET_METADATA);
  result.set_num_items_before_association(profiles.size() + cards.size());

  // base::Unretained is used because the callbacks are invoked synchronously.
  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncData& remote : sync_data) {
    DCHECK(remote.IsValid());
    DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, remote.GetDataType());
    switch (remote.GetSpecifics().wallet_metadata().type()) {
      case sync_pb::WalletMetadataSpecifics::ADDRESS:
        if (!MergeRemote(remote, address_updater, &profiles,
                         &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::CARD:
        if (!MergeRemote(remote, card_updater, &cards, &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  // The remainder of |profiles| were not listed in |sync_data|.
  for (const auto& it : profiles) {
    changes_to_sync.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        BuildSyncData(sync_pb::WalletMetadataSpecifics::ADDRESS, it.first,
                      *it.second)));
  }

  // The remainder of |cards| were not listed in |sync_data|.
  for (const auto& it : cards) {
    changes_to_sync.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                           BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                         it.first, *it.second)));
  }

  // The only operation that is performed locally in response to a sync is an
  // update. Adds and deletes are performed in response to changes to the Wallet
  // data.
  result.set_num_items_after_association(result.num_items_before_association());
  result.set_num_items_added(0);
  result.set_num_items_deleted(0);

  if (!changes_to_sync.empty())
    result.set_error(SendChangesToSyncServer(changes_to_sync));

  return result;
}

template <class DataType>
void AutofillWalletMetadataSyncableService::AutofillDataModelUpdated(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& type,
    const sync_pb::WalletMetadataSpecifics& remote,
    const DataType& local) {
  if (base::checked_cast<size_t>(remote.use_count()) < local.use_count() &&
      base::Time::FromInternalValue(remote.use_date()) < local.use_date()) {
    SendChangesToSyncServer(syncer::SyncChangeList(
        1, syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                              BuildSyncData(remote.type(), server_id, local))));
  }
}

}  // namespace autofill
