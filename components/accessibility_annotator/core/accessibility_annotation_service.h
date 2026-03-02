// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATION_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATION_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/entity_data_provider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

// Public service for accessing the accessibility annotator.
class AccessibilityAnnotationService : public KeyedService {
 public:
  explicit AccessibilityAnnotationService(
      std::unique_ptr<EntityDataProvider> entity_data_provider);
  ~AccessibilityAnnotationService() override;

  // EntityDataProvider methods forwarded:
  void AddObserver(EntityDataProvider::Observer* observer);
  void RemoveObserver(EntityDataProvider::Observer* observer);
  void GetEntities(EntityTypeEnumSet types,
                   base::OnceCallback<void(std::vector<Entity>)> callback);

 private:
  std::unique_ptr<EntityDataProvider> entity_data_provider_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATION_SERVICE_H_
