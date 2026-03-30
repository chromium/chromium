// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_

#include "base/functional/callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"

namespace content {
class WebContents;
}

namespace accessibility_annotator {

// Delegate interface for environment-specific (e.g., Chrome vs other embedders)
// UI implementations of the Accessibility Annotator First Feature Run.
class AccessibilityAnnotatorFirstRunClient {
 public:
  virtual ~AccessibilityAnnotatorFirstRunClient() = default;

  // Displays the informational UI for the remote annotator and invokes
  // `callback` with the result (e.g. accepted, declined, etc).
  virtual void ShowRemoteAnnotatorInfo(
      content::WebContents* web_contents,
      FirstRunInvocationSource invocation_source,
      base::OnceCallback<void(InfoResult)> callback) = 0;
};
}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_
