// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_STATIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_STATIC_TRIGGER_CONDITIONS_H_

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockStaticTriggerConditions : public StaticTriggerConditions {
 public:
  MockStaticTriggerConditions();
  ~MockStaticTriggerConditions() override;

  MOCK_METHOD1(Update, void(base::OnceCallback<void(void)> callback));
  MOCK_CONST_METHOD0(is_first_time_user, bool());
  MOCK_CONST_METHOD0(has_stored_login_credentials, bool());
  MOCK_CONST_METHOD1(is_in_experiment, bool(int experiment_id));
  MOCK_CONST_METHOD1(script_parameter_matches,
                     bool(const ScriptParameterMatchProto&));
  MOCK_CONST_METHOD0(has_results, bool());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_STATIC_TRIGGER_CONDITIONS_H_
