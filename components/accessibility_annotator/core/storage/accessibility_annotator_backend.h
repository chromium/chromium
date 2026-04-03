// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_pb {
class AccessibilityAnnotationSpecifics;
}

namespace accessibility_annotator {

class AccessibilityAnnotationSyncBridge;

class AccessibilityAnnotatorBackend : public KeyedService {
 public:
  struct ContentAnnotationsData {
    ContentAnnotationsData();
    ~ContentAnnotationsData();
    ContentAnnotationsData(ContentAnnotationsData&& other);
    ContentAnnotationsData& operator=(ContentAnnotationsData&& other);

    ContentAnnotationsData(const ContentAnnotationsData&) = delete;
    ContentAnnotationsData& operator=(const ContentAnnotationsData&) = delete;

    std::string page_title;
    std::optional<int> tab_id;
    base::DictValue annotations;
    base::DictValue classifier_results;
  };

  ~AccessibilityAnnotatorBackend() override = default;

  // Initializes the database. Must be called before any other methods.
  virtual void Init() = 0;

  // Returns DataTypeControllerDelegate for the accessibility annotation
  // datatype.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAccessibilityAnnotationControllerDelegate() = 0;

  // Reads from Content Annotations cache.
  virtual base::optional_ref<const ContentAnnotationsData>
  GetContentAnnotationsCacheData(const GURL& url) const = 0;

  // Writes to Content Annotations cache.
  virtual void SetContentAnnotationsCacheData(const GURL& url,
                                              ContentAnnotationsData data) = 0;

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
