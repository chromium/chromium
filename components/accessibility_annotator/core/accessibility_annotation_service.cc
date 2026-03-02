// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotation_service.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"

namespace accessibility_annotator {

AccessibilityAnnotationService::AccessibilityAnnotationService(
    std::unique_ptr<EntityDataProvider> entity_data_provider)
    : entity_data_provider_(std::move(entity_data_provider)) {
  CHECK(entity_data_provider_);
}

AccessibilityAnnotationService::~AccessibilityAnnotationService() = default;

void AccessibilityAnnotationService::AddObserver(
    EntityDataProvider::Observer* observer) {
  entity_data_provider_->AddObserver(observer);
}

void AccessibilityAnnotationService::RemoveObserver(
    EntityDataProvider::Observer* observer) {
  entity_data_provider_->RemoveObserver(observer);
}

void AccessibilityAnnotationService::GetEntities(
    EntityTypeEnumSet types,
    base::OnceCallback<void(std::vector<Entity>)> callback) {
  entity_data_provider_->GetEntities(types, std::move(callback));
}

}  // namespace accessibility_annotator
