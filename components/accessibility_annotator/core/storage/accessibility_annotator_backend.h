// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "url/gurl.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_pb {
class AccessibilityAnnotationSpecifics;
}  // namespace sync_pb

namespace accessibility_annotator {

class AccessibilityAnnotationSyncBridge;

class AccessibilityAnnotatorBackend : public KeyedService {
 public:
  struct ContentAnnotationsData;

  class Observer : public base::CheckedObserver {
   public:
    // Called when content annotations are added.
    virtual void OnContentAnnotationsAdded(
        history::VisitID visit_id,
        const ContentAnnotationsData& annotation_data) = 0;

    // Called when content annotations are deleted.
    virtual void OnContentAnnotationsDeleted(
        base::span<const history::VisitID> visit_ids) = 0;

    // Called when content annotations are cleared.
    virtual void OnContentAnnotationsCleared() = 0;
  };

  // TODO(crbug.com/501429617): Move this struct out of backend class.
  struct ContentAnnotationsData {
    ContentAnnotationsData();
    ~ContentAnnotationsData();
    ContentAnnotationsData(ContentAnnotationsData&& other);
    ContentAnnotationsData& operator=(ContentAnnotationsData&& other);

    ContentAnnotationsData(const ContentAnnotationsData&) = delete;
    ContentAnnotationsData& operator=(const ContentAnnotationsData&) = delete;

    std::string page_title;
    std::optional<int> tab_id;
    optimization_guide::proto::ContentAnnotation content_annotation;
    base::DictValue classifier_results;
    base::Time navigation_timestamp;
    history::VisitID visit_id = history::kInvalidVisitID;
    GURL url;
  };

  ~AccessibilityAnnotatorBackend() override = default;

  // Initializes the database. Must be called before any other methods.
  virtual void Init() = 0;

  // Returns DataTypeControllerDelegate for the accessibility annotation
  // datatype.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAccessibilityAnnotationControllerDelegate() = 0;

  // Adds an observer to the backend.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer from the backend.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Reads from Content Annotations cache.
  virtual base::optional_ref<const ContentAnnotationsData>
  GetContentAnnotationsCacheData(history::VisitID visit_id) const = 0;

  // Writes to Content Annotations cache.
  virtual void SetContentAnnotationsCacheData(history::VisitID visit_id,
                                              ContentAnnotationsData data) = 0;

  // Removes the entries with the given visit IDs from Content Annotations
  // cache.
  virtual void RemoveContentAnnotationsCacheData(
      base::span<const history::VisitID> visit_ids) = 0;

  // Clears the Content Annotations cache.
  virtual void ClearContentAnnotationsCache() = 0;

  // Pulls cache data into a base::Value for use in the debug UI.
  virtual base::Value GetDebugUICacheData() const = 0;

  // Returns sync annotations from the sync bridge that match the given entity
  // types.
  virtual void GetSyncAnnotationsByTypes(
      EntityTypeEnumSet types,
      base::OnceCallback<
          void(std::vector<sync_pb::AccessibilityAnnotationSpecifics>)>
          callback) = 0;

  // Returns `accessibility_annotation_sync_bridge_`.
  // TODO(crbug.com/489492084): This is currently used by
  // `DirectServerEntityProvider` to directly observe the sync bridge. Remove
  // this method once `DirectServerEntityProvider` is deprecated and removed.
  virtual AccessibilityAnnotationSyncBridge*
  accessibility_annotation_sync_bridge() = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
