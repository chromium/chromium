// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class MetadataChangeList;
class ModelError;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace autofill {

class AutofillProfileSyncDifferenceTracker;
class AutofillTable;
class AutofillWebDataService;
enum class AutofillProfileSyncChangeOrigin;

// Sync bridge implementation for AUTOFILL_PROFILE model type. Takes care of
// propagating local autofill profiles to other clients as well as incorporating
// profiles coming from other clients; and most notably resolving conflicts and
// merging duplicates.
//
// This is achieved by implementing the interface ModelTypeSyncBridge, which
// ClientTagBasedModelTypeProcessor will use to interact, ultimately, with the
// sync server. See
// https://www.chromium.org/developers/design-documents/sync/model-api/#implementing-modeltypesyncbridge
// for details.
class AutofillProfileSyncBridge
    : public base::SupportsUserData::Data,
      public AutofillWebDataServiceObserverOnDBSequence,
      public syncer::ModelTypeSyncBridge {
 public:
  AutofillProfileSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      const std::string& app_locale,
      AutofillWebDataBackend* backend);

  AutofillProfileSyncBridge(const AutofillProfileSyncBridge&) = delete;
  AutofillProfileSyncBridge& operator=(const AutofillProfileSyncBridge&) =
      delete;

  ~AutofillProfileSyncBridge() override;

  // Constructor that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|.
  static void CreateForWebDataServiceAndBackend(
      const std::string& app_locale,
      AutofillWebDataBackend* web_data_backend,
      AutofillWebDataService* web_data_service);

  // Retrieves the bridge from |web_data_service| which owns it.
  static syncer::ModelTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;

 private:
  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable();

  // Respond to local autofill profile entry changing by notifying sync of the
  // changes.
  void ActOnLocalChange(const AutofillProfileChange& change);

  // Flushes changes accumulated within |tracker| both to local and to sync.
  absl::optional<syncer::ModelError> FlushSyncTracker(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      AutofillProfileSyncDifferenceTracker* tracker);

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  // Locale needed for comparing autofill profiles when resolving conflicts.
  const std::string app_locale_;

  // AutofillProfileSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  base::ScopedObservation<AutofillWebDataBackend,
                          AutofillWebDataServiceObserverOnDBSequence>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_BRIDGE_H_
