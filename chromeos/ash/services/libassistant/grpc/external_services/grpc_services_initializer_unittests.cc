// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_initializer.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

class GrpcServicesInitializerTest : public testing::Test {
 public:
  GrpcServicesInitializerTest() = default;
  GrpcServicesInitializerTest(const GrpcServicesInitializerTest&) = delete;
  GrpcServicesInitializerTest& operator=(const GrpcServicesInitializerTest&) =
      delete;
  ~GrpcServicesInitializerTest() override = default;

 protected:
  std::unique_ptr<GrpcServicesInitializer> grpc_services_;

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_F(GrpcServicesInitializerTest, StartService) {
  // Should not crash at the end of the test.
  grpc_services_ = std::make_unique<GrpcServicesInitializer>(
      GetLibassistantServiceAddress(
          /*is_chromeos_device=*/false),
      GetAssistantServiceAddress(
          /*is_chromeos_device=*/false));
  grpc_services_->Start();
  grpc_services_.reset();
}

}  // namespace ash::libassistant
