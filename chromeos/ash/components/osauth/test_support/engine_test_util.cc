// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/test_support/engine_test_util.h"

namespace ash {

EngineTestBase::EngineTestBase() : core_(&mock_udac_) {
  user_manager::FakeUserManager::RegisterPrefs(prefs_.registry());
  user_manager_.Initialize();
}

EngineTestBase::~EngineTestBase() {
  user_manager_.Shutdown();
  user_manager_.Destroy();
}

}  // namespace ash
