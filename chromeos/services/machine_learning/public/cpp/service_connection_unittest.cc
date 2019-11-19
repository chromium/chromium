// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace machine_learning {
namespace {

class ServiceConnectionTest : public testing::Test {
 public:
  ServiceConnectionTest() = default;

  void SetUp() override { MachineLearningClient::InitializeFake(); }

  void TearDown() override { MachineLearningClient::Shutdown(); }

 protected:
  static void SetUpTestCase() {
    static base::Thread ipc_thread("ipc");
    ipc_thread.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    static mojo::core::ScopedIPCSupport ipc_support(
        ipc_thread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionTest);
};

// Tests that LoadBuiltinModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadBuiltinModel) {
  mojo::Remote<mojom::Model> model;
  mojom::BuiltinModelSpecPtr spec =
      mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL);
  ServiceConnection::GetInstance()->LoadBuiltinModel(
      std::move(spec), model.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests that LoadFlatBufferModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadFlatBufferModel) {
  mojo::Remote<mojom::Model> model;
  mojom::FlatBufferModelSpecPtr spec = mojom::FlatBufferModelSpec::New();
  ServiceConnection::GetInstance()->LoadFlatBufferModel(
      std::move(spec), model.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests the fake ML service for builtin model.
TEST_F(ServiceConnectionTest, FakeServiceConnectionForBuiltinModel) {
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);

  const double expected_value = 200.002;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});
  ServiceConnection::GetInstance()->LoadBuiltinModel(
      mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
      model.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::LoadModelResult result) {
            EXPECT_EQ(result, mojom::LoadModelResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  model->CreateGraphExecutor(
      graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        base::Optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
}

// Tests the fake ML service for flatbuffer model.
TEST_F(ServiceConnectionTest, FakeServiceConnectionForFlatBufferModel) {
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);

  const double expected_value = 200.002;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  ServiceConnection::GetInstance()->LoadFlatBufferModel(
      mojom::FlatBufferModelSpec::New(), model.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::LoadModelResult result) {
            EXPECT_EQ(result, mojom::LoadModelResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  model->CreateGraphExecutor(
      graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        base::Optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
}

}  // namespace
}  // namespace machine_learning
}  // namespace chromeos
