// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/threading/sequence_bound.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "url/gurl.h"

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace history {
class DeletionInfo;
class HistoryService;
struct VisitedURLInfo;
}  // namespace history

namespace accessibility_annotator {

class AccessibilityAnnotatorDatabase;

class AccessibilityAnnotatorBackendImpl
    : public AccessibilityAnnotatorBackend,
      public AccessibilityAnnotationSyncBridge::Observer,
      public history::HistoryServiceObserver {
 public:
  AccessibilityAnnotatorBackendImpl(
      history::HistoryService* history_service,
      os_crypt_async::OSCryptAsync* os_crypt_async,
      syncer::RepeatingDataTypeStoreFactory data_type_store_factory,
      const base::FilePath& db_path);

  ~AccessibilityAnnotatorBackendImpl() override;

  AccessibilityAnnotatorBackendImpl(const AccessibilityAnnotatorBackendImpl&) =
      delete;
  AccessibilityAnnotatorBackendImpl& operator=(
      const AccessibilityAnnotatorBackendImpl&) = delete;

  // KeyedService implementation.
  void Shutdown() override;

  // AccessibilityAnnotatorBackend implementation.
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAccessibilityAnnotationControllerDelegate() override;
  void AddObserver(AccessibilityAnnotatorBackend::Observer* observer) override;
  void RemoveObserver(
      AccessibilityAnnotatorBackend::Observer* observer) override;
  base::optional_ref<const ContentAnnotationsData>
  GetContentAnnotationsCacheData(history::VisitID visit_id) const override;
  void SetContentAnnotationsCacheData(history::VisitID visit_id,
                                      ContentAnnotationsData data) override;
  void RemoveContentAnnotationsCacheData(
      base::span<const history::VisitID> visit_ids) override;
  void ClearContentAnnotationsCache() override;
  base::Value GetDebugUICacheData() const override;

  const base::circular_deque<optimization_guide::proto::ContentAnnotation>&
  GetMergedMultipageAnnotationsForTesting() const {
    return merged_multipage_annotations_;
  }

  void GetSyncAnnotationsByTypes(
      EntityTypeEnumSet types,
      base::OnceCallback<void(
          std::vector<sync_pb::AccessibilityAnnotationSpecifics>)> callback)
      override;

  // AccessibilityAnnotationSyncBridge::Observer implementation.
  void OnAccessibilityAnnotationChanged() override;
  void OnAccessibilityAnnotationSyncBridgeLoaded() override;

  // history::HistoryServiceObserver implementation.
  void OnURLVisited(history::HistoryService* history_service,
                    const history::VisitedURLInfo& visited_url_info) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  // Returns `accessibility_annotation_sync_bridge_`.
  // TODO(crbug.com/489492084): This is currently used by
  // `DirectServerEntityProvider` to directly observe the sync bridge. Remove
  // this method once `DirectServerEntityProvider` is deprecated and removed.
  AccessibilityAnnotationSyncBridge* accessibility_annotation_sync_bridge()
      override;

 private:
  // Called in the backend constructor if the encryptor is available.
  // Initializes the database.
  void OnInitWithEncryptor(os_crypt_async::Encryptor encryptor);

  // Performs a lookback through recent pages with the same tab and eTLD+1 to
  // join annotations that span across multiple pages. The function merges
  // structured data from recent entries in reverse chronological order and
  // writes to `merged_multipage_annotations_`. This function is called only
  // when a confirmed status is detected in `data`.
  void ProcessConfirmedStatusLookback(const ContentAnnotationsData& data);

  // Deep merges `source_structured_data` into `target_structured_data`. For any
  // field that is set in both, the existing value in `target_structured_data`
  // takes precedence.
  // TODO(crbug.com/489690454): Consider moving merge logic to a separate file
  // if more structured data types are added.
  void MergeContentAnnotationStructuredData(
      const optimization_guide::proto::StructuredData& source_structured_data,
      optimization_guide::proto::StructuredData* target_structured_data);

  const base::FilePath db_path_;
  base::SequenceBound<AccessibilityAnnotatorDatabase> db_;
  std::unique_ptr<AccessibilityAnnotationSyncBridge>
      accessibility_annotation_sync_bridge_;

  // Stores annotations keyed by the visit ID they are associated with. The
  // cache size is `kContentAnnotatorMaxCacheAnnotations`. When the cache is
  // full, the least recently used entry is evicted.
  base::LRUCache<history::VisitID, ContentAnnotationsData>
      content_annotations_cache_;

  base::ScopedObservation<AccessibilityAnnotationSyncBridge,
                          AccessibilityAnnotationSyncBridge::Observer>
      sync_bridge_observation_{this};
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::ObserverList<AccessibilityAnnotatorBackend::Observer> observers_;

  // Holds multi-page merged annotations during confirmed status lookback.
  base::circular_deque<optimization_guide::proto::ContentAnnotation>
      merged_multipage_annotations_;

  base::WeakPtrFactory<AccessibilityAnnotatorBackendImpl> weak_ptr_factory_{
      this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_
