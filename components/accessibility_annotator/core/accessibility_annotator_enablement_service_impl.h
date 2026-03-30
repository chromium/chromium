// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImpl
    : public AccessibilityAnnotatorEnablementService {
 public:
  explicit AccessibilityAnnotatorEnablementServiceImpl();
  AccessibilityAnnotatorEnablementServiceImpl(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  AccessibilityAnnotatorEnablementServiceImpl& operator=(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  ~AccessibilityAnnotatorEnablementServiceImpl() override;

  // AccessibilityAnnotatorEnablementService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  RemoteAnnotatorEnablementState GetEnablementState() override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_
