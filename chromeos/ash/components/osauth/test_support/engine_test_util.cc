// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/test_support/engine_test_util.h"

#include <memory>

namespace ash {

EngineTestBase::EngineTestBase() : core_(&mock_udac_) {
  ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
      prefs_.registry());
  user_session_test_environment_ =
      std::make_unique<ash::test::UserSessionTestEnvironment>(&prefs_);
}

EngineTestBase::~EngineTestBase() = default;

}  // namespace ash
