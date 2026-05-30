// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/first_run/personal_context_first_run_client.h"
#include "components/personal_context/first_run/personal_context_first_run_types.h"

namespace content {
class WebContents;
}

namespace personal_context {

// Service that manages the First Feature Run for the Personal Context.
// It provides methods to trigger the first run flow.
// Clients can observe changes to the enablement state.
class PersonalContextFirstRunService : public KeyedService {
 public:
  using FirstRunTriggerCallback =
      base::OnceCallback<void(FirstRunTriggerResult)>;

  ~PersonalContextFirstRunService() override = default;

  // Triggers the first-run experience if the current profile is eligible but
  // has not completed it yet. The `callback` is invoked with the result of
  // the first-run trigger attempt (e.g., success, failure, or already
  // completed).
  virtual void MaybeTriggerFirstRun(content::WebContents* web_contents,
                                    FirstRunInvocationSource invocation_source,
                                    FirstRunTriggerCallback callback) = 0;

  // Called when the user has been shown the Personal Context notice in
  // Autofill.
  // TODO(b:517579158): Wire the notice UIs into this function.
  virtual void MarkPersonalContextInAutofillNoticeAsShown() = 0;

  // Called when the user has acknowledged the Personal Context notice in
  // Autofill.
  // TODO(b:517579158): Wire the notice UIs into this function.
  virtual void MarkPersonalContextInAutofillNoticeAsAcknowledged() = 0;

  // Returns true if the Personal Context notice should be shown in Autofill.
  // TODO(b:517579158): Wire the notice UIs into this function.
  virtual bool ShouldShowPersonalContextAutofillNotice() const = 0;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
