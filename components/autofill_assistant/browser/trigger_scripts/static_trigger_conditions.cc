// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

namespace autofill_assistant {

StaticTriggerConditions::StaticTriggerConditions() = default;
StaticTriggerConditions::~StaticTriggerConditions() = default;

bool StaticTriggerConditions::is_first_time_user() const {
  return false;
}

bool StaticTriggerConditions::has_stored_login_credentials() const {
  return false;
}

bool StaticTriggerConditions::is_in_experiment(int experiment_id) const {
  return false;
}

}  // namespace autofill_assistant
