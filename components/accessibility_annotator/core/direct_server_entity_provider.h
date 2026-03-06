// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/entity_data_provider.h"

namespace accessibility_annotator {

// Implementation of EntityDataProvider that reads directly from server
// entities.
class DirectServerEntityProvider : public EntityDataProvider {
 public:
  DirectServerEntityProvider();
  ~DirectServerEntityProvider() override;

  // EntityDataProvider:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetEntities(
      EntityTypeEnumSet types,
      base::OnceCallback<void(std::vector<Entity>)> callback) override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DIRECT_SERVER_ENTITY_PROVIDER_H_
