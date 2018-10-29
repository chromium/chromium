// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/autofill_specifics.pb.h"

namespace base {
class Location;
}

namespace syncer {
class SyncChangeProcessor;
class SyncErrorFactory;
}

namespace autofill {

class AutofillProfile;
class AutofillWebDataBackend;
class AutofillWebDataService;
class CreditCard;

// Syncs usage counts and last use dates (metadata) for Wallet cards and
// addresses (data).
//
// The sync server generates the data, and the client can only download it. No
// data upload is possible. Chrome generates the corresponding metadata locally
// and uses the sync server to propagate the metadata to the other instances of
// Chrome. See the design doc at https://goo.gl/LS2y6M for more details.
class AutofillWalletMetadataSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public AutofillWebDataServiceObserverOnDBSequence {
 public:
  ~AutofillWalletMetadataSyncableService() override;

  // Determines whether this bridge should be monitoring the Wallet data. This
  // should be called whenever the data bridge sync state changes.
  void OnWalletDataTrackingStateChanged(bool is_tracking);

  base::WeakPtr<AutofillWalletMetadataSyncableService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& changes_from_sync) override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;
  void CreditCardChanged(const CreditCardChange& change) override;
  void AutofillMultipleChanged() override;

  // Creates a new AutofillWalletMetadataSyncableService and hangs it off of
  // |web_data_service|, which takes ownership. This method should only be
  // called on |web_data_service|'s DB sequence. |web_data_backend| is expected
  // to outlive this object.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* web_data_backend,
      const std::string& app_locale);

  // Retrieves the AutofillWalletMetadataSyncableService stored on
  // |web_data_service|.
  static AutofillWalletMetadataSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

 protected:
  AutofillWalletMetadataSyncableService(
      AutofillWebDataBackend* web_data_backend,
      const std::string& app_locale);

  // Populates the provided |profiles| and |cards| with mappings from server ID
  // to server profiles and server cards read from disk. This data contains the
  // usage stats. Returns true on success.
  virtual bool GetLocalData(
      std::unordered_map<std::string, std::unique_ptr<AutofillProfile>>*
          profiles,
      std::unordered_map<std::string, std::unique_ptr<CreditCard>>* cards)
      const;

  // Updates the stats for |profile| stored on disk. Does not trigger
  // notifications that this profile was updated.
  virtual bool UpdateAddressStats(const AutofillProfile& profile);

  // Updates the stats for |credit_card| stored on disk. Does not trigger
  // notifications that this credit card was updated.
  virtual bool UpdateCardStats(const CreditCard& credit_card);

  // Sends the |changes_to_sync| to the sync server.
  virtual syncer::SyncError SendChangesToSyncServer(
      const syncer::SyncChangeList& changes_to_sync);

 private:
  // Merges local metadata with |sync_data|.
  //
  // Sends an "update" to the sync server if |sync_data| contains metadata that
  // is present locally, but local metadata has higher use count and later use
  // date.
  //
  // Sends a "create" to the sync server if |sync_data| does not have some local
  // metadata.
  //
  // Sends a "delete" to the sync server if |sync_data| contains metadata that
  // is not present locally.
  syncer::SyncMergeResult MergeData(const syncer::SyncDataList& sync_data);

  // Sends the autofill data model updates to the sync server if the local
  // version is more recent. Used for both profiles and credit cards.
  template <class DataType>
  void AutofillDataModelUpdated(
      const std::string& server_id,
      const sync_pb::WalletMetadataSpecifics::Type& type,
      const sync_pb::WalletMetadataSpecifics& remote,
      const DataType& local);

  base::ThreadChecker thread_checker_;
  AutofillWebDataBackend* web_data_backend_;  // Weak ref.
  ScopedObserver<AutofillWebDataBackend, AutofillWalletMetadataSyncableService>
      scoped_observer_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  // Local metadata plus metadata for the data that hasn't synced down yet.
  syncer::SyncDataList cache_;

  // Indicates whether we should rely on wallet data being actively synced. If
  // true, the service will prune metadata entries without corresponding wallet
  // data entry.
  bool track_wallet_data_;

  base::WeakPtrFactory<AutofillWalletMetadataSyncableService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncableService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_
