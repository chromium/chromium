// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_

#include "base/functional/callback.h"
#include "components/accessibility_annotator/first_run/personal_context_first_run_types.h"

namespace content {
class WebContents;
}

namespace personal_context {

// Delegate interface for environment-specific (e.g., Chrome vs other embedders)
// UI implementations of the Personal Context First Feature Run.
class PersonalContextFirstRunClient {
 public:
  virtual ~PersonalContextFirstRunClient() = default;

  // Displays the notice UI and invokes `callback` with the result
  // (e.g. accepted, declined, etc).
  virtual void ShowNotice(content::WebContents* web_contents,
                          FirstRunInvocationSource invocation_source,
                          base::OnceCallback<void(NoticeResult)> callback) = 0;
};
}  // namespace personal_context

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_
