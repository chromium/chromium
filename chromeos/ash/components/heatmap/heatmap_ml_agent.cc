// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/heatmap/heatmap_ml_agent.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"

namespace ash {

using chromeos::machine_learning::mojom::BuiltinModelId;
using chromeos::machine_learning::mojom::BuiltinModelSpec;
using chromeos::machine_learning::mojom::BuiltinModelSpecPtr;
using chromeos::machine_learning::mojom::ExecuteResult;
using chromeos::machine_learning::mojom::FloatList;
using chromeos::machine_learning::mojom::Int64List;
using chromeos::machine_learning::mojom::LoadModelResult;
using chromeos::machine_learning::mojom::Tensor;
using chromeos::machine_learning::mojom::TensorPtr;
using chromeos::machine_learning::mojom::ValueList;

namespace {

constexpr int kHeatmapHeight = 50;
constexpr int kHeatmapWidth = 32;

}  // namespace

HeatmapMlAgent::HeatmapMlAgent() = default;

HeatmapMlAgent::~HeatmapMlAgent() = default;

void HeatmapMlAgent::LazyInitialize() {
  if (!ml_service_) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  }
  if (!model_) {
    // Load the model.
    BuiltinModelSpecPtr spec =
        BuiltinModelSpec::New(BuiltinModelId::PONCHO_PALM_REJECTION_20230213);

    ml_service_->LoadBuiltinModel(
        std::move(spec), model_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&HeatmapMlAgent::OnLoadModel, base::Unretained(this)));
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(
        chromeos::machine_learning::mojom::GraphExecutorOptions::New(),
        executor_.BindNewPipeAndPassReceiver(), base::DoNothing());
    executor_.set_disconnect_handler(base::BindOnce(
        &HeatmapMlAgent::OnConnectionError, base::Unretained(this)));
  }
}

void HeatmapMlAgent::Execute(const std::vector<double>& data,
                             ExecuteCallback callback) {
  if (data.size() != kHeatmapHeight * kHeatmapWidth) {
    DVLOG(1) << "Heatmap data size is incorrect, expect "
             << kHeatmapHeight * kHeatmapWidth << ", got " << data.size();
    std::move(callback).Run(absl::nullopt);
    return;
  }

  LazyInitialize();

  base::flat_map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value =
      std::vector<int64_t>{1, kHeatmapHeight, kHeatmapWidth, 1};
  tensor->data = ValueList::NewFloatList(FloatList::New(data));
  inputs.emplace("input", std::move(tensor));
  std::vector<std::string> outputs({"output"});
  std::vector<int64_t> expected_shape{1, 1};

  executor_->Execute(
      std::move(inputs), std::move(outputs),
      base::BindOnce(&HeatmapMlAgent::OnExecuteDone, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void HeatmapMlAgent::OnExecuteDone(
    ExecuteCallback callback,
    ExecuteResult result,
    absl::optional<std::vector<TensorPtr>> outputs) {
  if (result == ExecuteResult::OK && outputs.has_value() &&
      outputs->size() == 1) {
    auto& output_data = (outputs.value())[0]->data;
    if (output_data->is_float_list() &&
        output_data->get_float_list()->value.size() == 1) {
      std::move(callback).Run(output_data->get_float_list()->value[0]);
      return;
    }
  }
  std::move(callback).Run(absl::nullopt);
}

void HeatmapMlAgent::OnLoadModel(LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    DVLOG(1) << "Failed to load the heatmap model, error: " << result;
    executor_.reset();
    model_.reset();
  }
}

void HeatmapMlAgent::OnConnectionError() {
  DVLOG(1) << "Mojo connection for ML service is closed!";
  executor_.reset();
  model_.reset();
}

}  // namespace ash
