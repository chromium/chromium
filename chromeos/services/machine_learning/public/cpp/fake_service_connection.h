// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace machine_learning {

// Fake implementation of chromeos::machine_learning::ServiceConnection.
// Handles LoadModel (and Model::CreateGraphExecutor) by binding to itself.
// Handles GraphExecutor::Execute by always returning the value specified by
// a previous call to SetOutputValue.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl : public ServiceConnection,
                                  public mojom::Model,
                                  public mojom::GraphExecutor {
 public:
  FakeServiceConnectionImpl();
  ~FakeServiceConnectionImpl() override;

  // It's safe to execute LoadBuiltinModel and LoadFlatBufferModel for multi
  // times, but all the receivers will be bound to the same instance.
  void LoadBuiltinModel(mojom::BuiltinModelSpecPtr spec,
                        mojo::PendingReceiver<mojom::Model> receiver,
                        mojom::MachineLearningService::LoadBuiltinModelCallback
                            callback) override;
  void LoadFlatBufferModel(
      mojom::FlatBufferModelSpecPtr spec,
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadFlatBufferModelCallback callback)
      override;

  // mojom::Model:
  void CreateGraphExecutor(
      mojo::PendingReceiver<mojom::GraphExecutor> receiver,
      mojom::Model::CreateGraphExecutorCallback callback) override;

  // mojom::GraphExecutor:
  // Execute() will return the tensor set by SetOutputValue() as the output.
  void Execute(base::flat_map<std::string, mojom::TensorPtr> inputs,
               const std::vector<std::string>& output_names,
               mojom::GraphExecutor::ExecuteCallback callback) override;

  // Call SetOutputValue() before Execute() to set the output tensor.
  void SetOutputValue(const std::vector<int64_t>& shape,
                      const std::vector<double>& value);

 private:
  mojo::ReceiverSet<mojom::Model> model_receivers_;
  mojo::ReceiverSet<mojom::GraphExecutor> graph_receivers_;
  mojom::TensorPtr execute_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeServiceConnectionImpl);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
