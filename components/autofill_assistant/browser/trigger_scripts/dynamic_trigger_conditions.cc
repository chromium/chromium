// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"

namespace autofill_assistant {

DynamicTriggerConditions::DynamicTriggerConditions() = default;
DynamicTriggerConditions::~DynamicTriggerConditions() = default;

base::Optional<bool> DynamicTriggerConditions::GetSelectorMatches(
    const Selector& selector) const {
  return false;
}

void DynamicTriggerConditions::Update(WebController* web_controller,
                                      base::OnceCallback<void(void)> callback) {
}

}  // namespace autofill_assistant
