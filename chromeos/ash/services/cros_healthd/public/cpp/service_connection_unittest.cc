// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cros_healthd {
namespace {

class CrosHealthdServiceConnectionTest : public testing::Test {
 public:
  CrosHealthdServiceConnectionTest() = default;

  CrosHealthdServiceConnectionTest(const CrosHealthdServiceConnectionTest&) =
      delete;
  CrosHealthdServiceConnectionTest& operator=(
      const CrosHealthdServiceConnectionTest&) = delete;

  void SetUp() override { FakeCrosHealthd::Initialize(); }

  void TearDown() override { FakeCrosHealthd::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

// Test that we can get diagnostics service.
TEST_F(CrosHealthdServiceConnectionTest, GetDiagnosticsService) {
  auto* service = ServiceConnection::GetInstance()->GetDiagnosticsService();
  ServiceConnection::GetInstance()->FlushForTesting();
  EXPECT_TRUE(service);
}

// Test that we can get probe service.
TEST_F(CrosHealthdServiceConnectionTest, GetProbeService) {
  auto* service = ServiceConnection::GetInstance()->GetProbeService();
  ServiceConnection::GetInstance()->FlushForTesting();
  EXPECT_TRUE(service);
}

// Test that we can get event service.
TEST_F(CrosHealthdServiceConnectionTest, GetEventService) {
  auto* service = ServiceConnection::GetInstance()->GetEventService();
  ServiceConnection::GetInstance()->FlushForTesting();
  EXPECT_TRUE(service);
}

// Test that we can get event service.
TEST_F(CrosHealthdServiceConnectionTest, GetRoutinesService) {
  auto* service = ServiceConnection::GetInstance()->GetRoutinesService();
  ServiceConnection::GetInstance()->FlushForTesting();
  EXPECT_TRUE(service);
}

// Test that we can bind diagnostics service.
TEST_F(CrosHealthdServiceConnectionTest, BindDiagnosticsService) {
  mojo::Remote<mojom::CrosHealthdDiagnosticsService> remote;
  ServiceConnection::GetInstance()->BindDiagnosticsService(
      remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

// Test that we can bind probe service.
TEST_F(CrosHealthdServiceConnectionTest, BindProbeService) {
  mojo::Remote<mojom::CrosHealthdProbeService> remote;
  ServiceConnection::GetInstance()->BindProbeService(
      remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

}  // namespace
}  // namespace ash::cros_healthd
