// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory_impl.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

namespace network_health_mojom = ::chromeos::network_health::mojom;
namespace network_diag_mojom = ::chromeos::network_diagnostics::mojom;

class CrosHealthdInternalServiceFactoryImplTest;

class MockInternalServiceFactoryImpl : public InternalServiceFactoryImpl {
  using InternalServiceFactoryImpl::InternalServiceFactoryImpl;
};

class CrosHealthdInternalServiceFactoryImplTest : public testing::Test {
 public:
  CrosHealthdInternalServiceFactoryImplTest() = default;
  CrosHealthdInternalServiceFactoryImplTest(
      const CrosHealthdInternalServiceFactoryImplTest&) = delete;
  CrosHealthdInternalServiceFactoryImplTest& operator=(
      const CrosHealthdInternalServiceFactoryImplTest&) = delete;

  void SetUp() override {
    mock_internal_service_factory_.BindReceiver(
        internal_service_factory_remote_.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(internal_service_factory_remote_.is_connected());
  }

  void Flush() { internal_service_factory_remote_.FlushForTesting(); }

  InternalServiceFactoryImpl* mock_internal_service_factory() {
    return &mock_internal_service_factory_;
  }

  mojom::CrosHealthdInternalServiceFactory* internal_service_factory_remote() {
    return internal_service_factory_remote_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  MockInternalServiceFactoryImpl mock_internal_service_factory_;
  mojo::Remote<mojom::CrosHealthdInternalServiceFactory>
      internal_service_factory_remote_;
};

TEST_F(CrosHealthdInternalServiceFactoryImplTest, GetNetworkHealthService) {
  mojo::Remote<network_health_mojom::NetworkHealthService> remote;
  internal_service_factory_remote()->GetNetworkHealthService(
      remote.BindNewPipeAndPassReceiver());
  remote.reset();
  internal_service_factory_remote()->GetNetworkHealthService(
      remote.BindNewPipeAndPassReceiver());

  int count = 0;
  auto callback = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<network_health_mojom::NetworkHealthService>
              receiver) { ++count; });
  mock_internal_service_factory()->SetBindNetworkHealthServiceCallback(
      callback);
  Flush();
  ASSERT_EQ(count, 2);

  remote.reset();
  internal_service_factory_remote()->GetNetworkHealthService(
      remote.BindNewPipeAndPassReceiver());
  remote.reset();
  internal_service_factory_remote()->GetNetworkHealthService(
      remote.BindNewPipeAndPassReceiver());
  Flush();
  ASSERT_EQ(count, 4);
}

TEST_F(CrosHealthdInternalServiceFactoryImplTest, GetNetworkDiagService) {
  mojo::Remote<network_diag_mojom::NetworkDiagnosticsRoutines> remote;
  internal_service_factory_remote()->GetNetworkDiagnosticsRoutines(
      remote.BindNewPipeAndPassReceiver());
  remote.reset();
  internal_service_factory_remote()->GetNetworkDiagnosticsRoutines(
      remote.BindNewPipeAndPassReceiver());

  int count = 0;
  auto callback = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<network_diag_mojom::NetworkDiagnosticsRoutines>
              receiver) { ++count; });
  mock_internal_service_factory()->SetBindNetworkDiagnosticsRoutinesCallback(
      callback);
  Flush();
  ASSERT_EQ(count, 2);

  remote.reset();
  internal_service_factory_remote()->GetNetworkDiagnosticsRoutines(
      remote.BindNewPipeAndPassReceiver());
  remote.reset();
  internal_service_factory_remote()->GetNetworkDiagnosticsRoutines(
      remote.BindNewPipeAndPassReceiver());
  Flush();
  ASSERT_EQ(count, 4);
}

}  // namespace
}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
