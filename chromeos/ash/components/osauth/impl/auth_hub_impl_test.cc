// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr AshAuthFactor kOneFactor = AshAuthFactor::kGaiaPassword;

using base::test::RunOnceCallback;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

class AuthHubTestMode : public ::testing::Test {
 protected:
  AuthHubTestMode() {
    parts_ = AuthPartsImpl::CreateTestInstance();
    parts_->SetAuthHub(std::make_unique<AuthHubImpl>());

    auto factory = std::make_unique<StrictMock<MockAuthFactorEngineFactory>>();
    auto engine = std::make_unique<StrictMock<MockAuthFactorEngine>>();

    engine_ = engine.get();

    EXPECT_CALL(*factory, GetFactor()).WillRepeatedly(Return(kOneFactor));
    EXPECT_CALL(*engine, GetFactor()).WillRepeatedly(Return(kOneFactor));
    EXPECT_CALL(*factory, CreateEngine(_))
        .WillOnce(Return(ByMove(std::move(engine))));

    parts_->RegisterEngineFactory(std::move(factory));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<AuthPartsImpl> parts_;

  base::raw_ptr<MockAuthFactorEngine> engine_;
};

TEST_F(AuthHubTestMode, CheckEnsureInitialized) {
  base::test::TestFuture<void> init_future;

  AuthHub::Get()->EnsureInitialized(init_future.GetCallback());

  AuthFactorEngine::CommonInitCallback init_callback;
  EXPECT_CALL(*engine_, InitializeCommon(_))
      .WillOnce(MoveArg<0>(&init_callback));

  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);

  EXPECT_FALSE(init_future.IsReady());

  std::move(init_callback).Run(kOneFactor);

  EXPECT_TRUE(init_future.IsReady());
}

}  // namespace ash
