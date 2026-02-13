// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_H_

#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// Class for extracting entity instances from the Accessibility Annotator.
// The Accessibility Annotator provides data from sources like Sian and
// packages them into entity instances for Autofill.
class AccessibilityAnnotatorDataAdapter : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the entities in the adapter have changed.
    virtual void OnAccessibilityAnnotatorDataChanged(
        AccessibilityAnnotatorDataAdapter& adapter) = 0;
  };

  AccessibilityAnnotatorDataAdapter();
  ~AccessibilityAnnotatorDataAdapter() override;

  // Returns all entity instances from the Accessibility Annotator.
  std::vector<EntityInstance> GetEntityInstances();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_H_
