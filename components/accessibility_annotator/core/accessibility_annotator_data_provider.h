// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDataProvider : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when one or more entity types have been updated, added, or
    // removed. Observers should then call `GetEntities` to retrieve the
    // updated data for the relevant types.
    virtual void OnAccessibilityAnnotatorEntitiesChanged(
        EntityTypeEnumSet types) = 0;
  };

  AccessibilityAnnotatorDataProvider();
  ~AccessibilityAnnotatorDataProvider() override;

  virtual void AddObserver(Observer* observer) = 0;

  virtual void RemoveObserver(Observer* observer) = 0;

  // Asynchronously retrieves all entities of the specified `types` that are
  // available.
  virtual void GetEntities(
      EntityTypeEnumSet types,
      base::OnceCallback<void(std::vector<Entity>)> callback) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_H_
