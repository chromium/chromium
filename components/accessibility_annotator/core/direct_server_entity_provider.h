// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/entity_data_provider.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorBackend;

// Implementation of EntityDataProvider that directly reads and provides server
// entities. Owned by AccessibilityAnnotationService.
// TODO(crbug.com/489492084): This class is a temporary solution until server
// entities are processed and provided via AnnotationReducer instead.
class DirectServerEntityProvider
    : public EntityDataProvider,
      public AccessibilityAnnotationSyncBridge::Observer {
 public:
  explicit DirectServerEntityProvider(AccessibilityAnnotatorBackend& backend);
  ~DirectServerEntityProvider() override;

  // EntityDataProvider:
  void AddObserver(EntityDataProvider::Observer* observer) override;
  void RemoveObserver(EntityDataProvider::Observer* observer) override;
  void GetEntities(
      EntityTypeEnumSet types,
      base::OnceCallback<void(std::vector<Entity>)> callback) override;

  // AccessibilityAnnotationSyncBridge::Observer:
  void OnAccessibilityAnnotationSyncBridgeLoaded() override;
  void OnAccessibilityAnnotationChanged() override;

  void NotifyObservers();

 private:
  base::ObserverList<EntityDataProvider::Observer> observers_;

  // `backend_` is owned by the BrowserContext and is guaranteed to outlive this
  // object due to the `DependsOn` in the AccessibilityAnnotationServiceFactory
  // that creates AccessibilityAnnotationService which owns this object.
  // Backend is used to access underlying storage, mainly the sync bridge for
  // direct access to server entities.
  // The sync bridge owned by the backend is created on the UI thread and thus
  // the model thread of the sync bridge is on the UI thread.
  raw_ref<AccessibilityAnnotatorBackend> backend_;

  // Observation of the sync bridge.
  base::ScopedObservation<AccessibilityAnnotationSyncBridge,
                          AccessibilityAnnotationSyncBridge::Observer>
      sync_bridge_observation_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_
