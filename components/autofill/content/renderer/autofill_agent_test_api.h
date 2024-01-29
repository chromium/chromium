// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/autofill_agent.h"

namespace autofill {

class AutofillAgentTestApi {
 public:
  explicit AutofillAgentTestApi(AutofillAgent* agent) : agent_(*agent) {}

  FormTracker& form_tracker() { return *agent_->form_tracker_; }
  void set_form_tracker(std::unique_ptr<FormTracker> form_tracker) {
    agent_->form_tracker_ = std::move(form_tracker);
  }

  std::optional<FormData> last_interacted_saved_state() {
    return agent_->last_interacted_.saved_state;
  }

 private:
  const raw_ref<AutofillAgent> agent_;
};

inline AutofillAgentTestApi test_api(AutofillAgent& agent) {
  return AutofillAgentTestApi(&agent);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_
