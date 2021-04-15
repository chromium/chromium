// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/mock_static_trigger_conditions.h"
#include "url/gurl.h"

namespace autofill_assistant {

MockStaticTriggerConditions::MockStaticTriggerConditions()
    : StaticTriggerConditions(nullptr, nullptr, GURL()) {}
MockStaticTriggerConditions::~MockStaticTriggerConditions() = default;

}  // namespace autofill_assistant
