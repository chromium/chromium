// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_client.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

namespace accessibility_annotator {

class AccessibilityAnnotatorFirstRunServiceImpl
    : public AccessibilityAnnotatorFirstRunService {
 public:
  explicit AccessibilityAnnotatorFirstRunServiceImpl(
      std::unique_ptr<AccessibilityAnnotatorFirstRunClient> client);
  AccessibilityAnnotatorFirstRunServiceImpl(
      const AccessibilityAnnotatorFirstRunServiceImpl&) = delete;
  AccessibilityAnnotatorFirstRunServiceImpl& operator=(
      const AccessibilityAnnotatorFirstRunServiceImpl&) = delete;
  ~AccessibilityAnnotatorFirstRunServiceImpl() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  RemoteAnnotatorEnablementState GetEnablementState() override;

  void MaybeTriggerFirstRun(
      content::WebContents* web_contents,
      FirstRunInvocationSource invocation_source,
      base::OnceCallback<void(FirstRunTriggerResult)> callback) override;

 private:
  void OnInfoDialogCompleted(
      base::OnceCallback<void(FirstRunTriggerResult)> callback,
      InfoResult result);

  std::unique_ptr<AccessibilityAnnotatorFirstRunClient> client_;
  base::ObserverList<Observer> observers_;
  RemoteAnnotatorEnablementState current_state_ =
      RemoteAnnotatorEnablementState::kDisabledPendingInfo;

  base::WeakPtrFactory<AccessibilityAnnotatorFirstRunServiceImpl>
      weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_IMPL_H_
