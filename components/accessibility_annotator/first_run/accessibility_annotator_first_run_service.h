// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_client.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

namespace accessibility_annotator {

// Service that manages the First Feature Run for the Accessibility
// Annotator. It tracks the enablement state of the remote annotator feature and
// provides methods to trigger the first run flow. Clients can observe changes
// to the enablement state.
class AccessibilityAnnotatorFirstRunService : public KeyedService {
 public:
  using FirstRunTriggerCallback =
      base::OnceCallback<void(FirstRunTriggerResult)>;

  ~AccessibilityAnnotatorFirstRunService() override = default;

  // Triggers the first-run experience if the current profile is eligible but
  // has not completed it yet. The `callback` is invoked with the result of
  // the first-run trigger attempt (e.g., success, failure, or already
  // completed).
  virtual void MaybeTriggerFirstRun(content::WebContents* web_contents,
                                    FirstRunInvocationSource invocation_source,
                                    FirstRunTriggerCallback callback) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_H_
