// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_

#include "components/autofill_assistant/browser/client.h"

namespace autofill_assistant {

// Provides easy access to the values of static trigger conditions. Static
// trigger conditions do not depend on the current state of the DOM, as opposed
// to dynamic element conditions.
class StaticTriggerConditions {
 public:
  StaticTriggerConditions();
  ~StaticTriggerConditions();

  // TODO(b/171776026): implement this stub.
  virtual void Init(Client* client) {}
  virtual bool is_first_time_user() const;
  virtual bool has_stored_login_credentials() const;
  virtual bool is_in_experiment(int experiment_id) const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
