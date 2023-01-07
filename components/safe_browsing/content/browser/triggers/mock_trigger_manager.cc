// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/triggers/mock_trigger_manager.h"

namespace safe_browsing {

MockTriggerManager::MockTriggerManager() : TriggerManager(nullptr, nullptr) {}

MockTriggerManager::~MockTriggerManager() {}

}  // namespace safe_browsing
