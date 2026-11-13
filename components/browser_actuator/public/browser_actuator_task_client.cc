// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/browser_actuator_task_client.h"

#include <string_view>

namespace browser_actuator {

BrowserActuatorTaskClient::Observer::Observer() = default;

BrowserActuatorTaskClient::Observer::~Observer() = default;

void BrowserActuatorTaskClient::Observer::OnTaskStarted() {}

void BrowserActuatorTaskClient::Observer::OnWorklogUpdate(
    std::string_view step_description) {}

BrowserActuatorTaskClient::BrowserActuatorTaskClient() = default;

BrowserActuatorTaskClient::~BrowserActuatorTaskClient() = default;

}  // namespace browser_actuator
