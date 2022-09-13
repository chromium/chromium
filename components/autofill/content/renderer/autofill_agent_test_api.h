// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_

#include "components/autofill/content/renderer/autofill_agent.h"

namespace autofill {

class AutofillAgentTestApi {
 public:
  explicit AutofillAgentTestApi(AutofillAgent* agent) : agent_(agent) {
    DCHECK(agent_);
  }

  void DidAssociateFormControlsDynamically() {
    agent_->DidAssociateFormControlsDynamically();
  }

 private:
  AutofillAgent* agent_;  // Not null.
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_TEST_API_H_
