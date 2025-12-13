// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/public/cpp/browser/system_signals_service_host_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/device_signals/core/browser/mock_system_signals_service_host.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "components/device_signals/core/common/signals_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace system_signals {

namespace {

class MockSystemSignalsServiceHostObserver
    : public device_signals::SystemSignalsServiceHost::Observer {
 public:
  MockSystemSignalsServiceHostObserver() = default;
  ~MockSystemSignalsServiceHostObserver() override = default;

  MOCK_METHOD(void, OnServiceDisconnect, (), (override));
};

}  // namespace

class SystemSignalsServiceHostImplTest : public testing::Test {
 public:
  SystemSignalsServiceHostImplTest() = default;
  ~SystemSignalsServiceHostImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_signals::features::
            kSystemSignalCollectionImprovementEnabled);

    service_host_impl_ = std::make_unique<SystemSignalsServiceHostImpl>();

    service_host_impl_->AddObserver(&mock_observer_);

    service_host_impl_->BindRemoteForTesting(
        mock_service_.BindNewPipeAndPassRemote());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  StrictMock<MockSystemSignalsServiceHostObserver> mock_observer_;
  StrictMock<device_signals::MockSystemSignalsService> mock_service_;
  std::unique_ptr<SystemSignalsServiceHostImpl> service_host_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that GetService() returns a valid service.
TEST_F(SystemSignalsServiceHostImplTest, GetService) {
  auto* service = service_host_impl_->GetService();
  ASSERT_TRUE(service);
}

// Tests that observers are notified when the service disconnects.
TEST_F(SystemSignalsServiceHostImplTest, ServiceDisconnect_NotifiesObservers) {
  auto* service = service_host_impl_->GetService();
  ASSERT_TRUE(service);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_observer_, OnServiceDisconnect()).WillOnce([&run_loop]() {
    std::move(run_loop.QuitClosure()).Run();
  });

  mock_service_.SimulateDisconnect();

  run_loop.Run();
}

// Tests that no observers are notified when the service disconnects and the
// observer was already removed.
TEST_F(SystemSignalsServiceHostImplTest,
       ServiceDisconnect_NoObservers_NoObserversNotified) {
  service_host_impl_->RemoveObserver(&mock_observer_);
  auto* service = service_host_impl_->GetService();
  ASSERT_TRUE(service);

  EXPECT_CALL(mock_observer_, OnServiceDisconnect()).Times(0);

  mock_service_.SimulateDisconnect();
}

// Tests that no observers are notified when the service disconnects and the
// feature is disabled.
TEST_F(SystemSignalsServiceHostImplTest,
       ServiceDisconnect_FeatureDisabled_NoObserversNotified) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      enterprise_signals::features::kSystemSignalCollectionImprovementEnabled);
  auto* service = service_host_impl_->GetService();
  ASSERT_TRUE(service);

  EXPECT_CALL(mock_observer_, OnServiceDisconnect()).Times(0);

  mock_service_.SimulateDisconnect();
}

}  // namespace system_signals
