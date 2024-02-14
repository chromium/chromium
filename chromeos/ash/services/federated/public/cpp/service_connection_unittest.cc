// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/federated/public/cpp/service_connection.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"
#include "chromeos/ash/services/federated/public/mojom/tables.mojom.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::federated {
namespace {

using chromeos::federated::mojom::Example;
using chromeos::federated::mojom::ExamplePtr;
using chromeos::federated::mojom::Features;

// Create an ExamplePtr for testing. It must contain real value to avoid
// VALIDATION_ERROR_UNEXPECTED_NULL_POINTER failure in dry run.
ExamplePtr CreateExample() {
  ExamplePtr example = Example::New();
  example->features = Features::New();
  auto& feature_map = example->features->feature;
  feature_map["int_feature1"] = CreateInt64List({1, 2, 3, 4, 5});
  feature_map["int_feature2"] = CreateInt64List({10, 20, 30, 40, 50});

  return example;
}

class FederatedServiceConnectionTest : public testing::Test {
 public:
  FederatedServiceConnectionTest() = default;
  FederatedServiceConnectionTest(const FederatedServiceConnectionTest&) =
      delete;
  FederatedServiceConnectionTest& operator=(
      const FederatedServiceConnectionTest&) = delete;

  // testing::Test:
  void SetUp() override { FederatedClient::InitializeFake(); }

  void TearDown() override { FederatedClient::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::core::ScopedIPCSupport ipc_support_{
      task_environment_.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN};
};

// Tests that BindReceiver runs OK (no crash) in a basic Mojo environment.
TEST_F(FederatedServiceConnectionTest, BindReceiver) {
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service;
  ServiceConnection::GetInstance()->BindReceiver(
      federated_service.BindNewPipeAndPassReceiver());
}

// Tests that FakeServiceConnection can handle BindReceiver and the bound
// receiver can call ReportExampleToTable successfully.
TEST_F(FederatedServiceConnectionTest, FakeServiceConnection) {
  FakeServiceConnectionImpl fake_service_connection;
  ScopedFakeServiceConnectionForTest scoped_fake_for_test(
      &fake_service_connection);

  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service;
  ServiceConnection::GetInstance()->BindReceiver(
      federated_service.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(federated_service.is_bound());
  EXPECT_TRUE(federated_service.is_connected());

  federated_service->ReportExampleToTable(
      chromeos::federated::mojom::FederatedExampleTableId::UNKNOWN,
      CreateExample());
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash::federated
