// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_

#include <memory>
#include <string>

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
  void Init() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAccessibilityAnnotationControllerDelegate() override;
  void AddObserver(AccessibilityAnnotatorBackend::Observer* observer) override;
  void RemoveObserver(
      AccessibilityAnnotatorBackend::Observer* observer) override;
  base::optional_ref<const ContentAnnotationsData>
  GetContentAnnotationsCacheData(const GURL& url) const override;
  void SetContentAnnotationsCacheData(const GURL& url,
                                      ContentAnnotationsData data) override;
  void RemoveContentAnnotationsCacheData(base::span<const GURL> urls) override;
  void ClearContentAnnotationsCache() override;
  base::Value GetDebugUICacheData() const override;
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
  const base::FilePath db_path_;
  base::SequenceBound<AccessibilityAnnotatorDatabase> db_;
  std::unique_ptr<AccessibilityAnnotationSyncBridge>
      accessibility_annotation_sync_bridge_;

  // Stores annotations keyed by the URL they are associated with. The cache
  // size is `kContentAnnotatorMaxCacheAnnotations`. When the cache is full, the
  // least recently used entry is evicted.
  base::LRUCache<GURL, ContentAnnotationsData> content_annotations_cache_;

  base::ScopedObservation<AccessibilityAnnotationSyncBridge,
                          AccessibilityAnnotationSyncBridge::Observer>
      sync_bridge_observation_{this};
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::ObserverList<AccessibilityAnnotatorBackend::Observer> observers_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_IMPL_H_
