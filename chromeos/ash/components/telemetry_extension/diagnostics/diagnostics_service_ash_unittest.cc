// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DiagnosticsServiceAshTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override { cros_healthd::FakeCrosHealthd::Initialize(); }
  void TearDown() override { cros_healthd::FakeCrosHealthd::Shutdown(); }

  crosapi::mojom::DiagnosticsServiceProxy* diagnostics_service() {
    return remote_diagnostics_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  mojo::Remote<crosapi::mojom::DiagnosticsService> remote_diagnostics_service_;
  std::unique_ptr<crosapi::mojom::DiagnosticsService> diagnostics_service_{
      DiagnosticsServiceAsh::Factory::Create(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver())};
};

}  // namespace ash
