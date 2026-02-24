// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_IMPL_H_

#include <vector>

#include "base/observer_list.h"
#include "components/accessibility_annotator/core/accessibility_annotator_data_provider.h"
#include "components/accessibility_annotator/core/data_models/entity.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDataProviderImpl
    : public AccessibilityAnnotatorDataProvider {
 public:
  AccessibilityAnnotatorDataProviderImpl();
  ~AccessibilityAnnotatorDataProviderImpl() override;

  // AccessibilityAnnotatorDataProvider:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetEntities(
      EntityTypeEnumSet types,
      base::OnceCallback<void(std::vector<Entity>)> callback) override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_IMPL_H_
