// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/direct_server_entity_provider.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/accessibility_annotator/core/data_models/entity_converter.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

DirectServerEntityProvider::DirectServerEntityProvider(
    AccessibilityAnnotatorBackend& backend)
    : backend_(backend) {
  if (backend.accessibility_annotation_sync_bridge()) {
    sync_bridge_observation_.Observe(
        backend.accessibility_annotation_sync_bridge());
  }
}

DirectServerEntityProvider::~DirectServerEntityProvider() = default;

void DirectServerEntityProvider::AddObserver(
    EntityDataProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void DirectServerEntityProvider::RemoveObserver(
    EntityDataProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DirectServerEntityProvider::GetEntities(
    EntityTypeEnumSet types,
    base::OnceCallback<void(std::vector<Entity>)> callback) {
  auto* bridge = backend_->accessibility_annotation_sync_bridge();
  if (!bridge) {
    std::move(callback).Run({});
    return;
  }

  std::vector<Entity> entities;
  for (const auto& specifics : bridge->GetAllAnnotations()) {
    std::optional<Entity> entity = CreateEntityFromSpecifics(specifics);
    if (entity.has_value() && types.Has(entity->GetType())) {
      entities.push_back(std::move(*entity));
    }
  }

  std::move(callback).Run(std::move(entities));
}

void DirectServerEntityProvider::OnAccessibilityAnnotationSyncBridgeLoaded() {
  // TODO(crbug.com/486856790): Notify observers for data change on loaded as
  // incremental sync is not supported.
  NotifyObservers();
}

void DirectServerEntityProvider::OnAccessibilityAnnotationChanged() {
  // The model thread of the sync bridge is on the UI thread as it is created
  // by the AccessibilityAnnotationBackend in the constructor on the UI thread.
  // Therefore, we can directly notify the observers on the UI thread.
  NotifyObservers();
}

void DirectServerEntityProvider::NotifyObservers() {
  for (EntityDataProvider::Observer& observer : observers_) {
    // TODO(crbug.com/486856790): Consider more granular notifications once
    // incremental sync is supported.
    observer.OnEntityDataChanged(*this, EntityTypeEnumSet::All());
  }
}

}  // namespace accessibility_annotator
