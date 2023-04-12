// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_ML_AGENT_H_
#define CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_ML_AGENT_H_

#include "base/functional/callback.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-shared.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A class that sends heatmap palm rejection request to ML service, parses the
// response and calls a provided callback method.
class HeatmapMlAgent {
 public:
  HeatmapMlAgent();
  ~HeatmapMlAgent();

  using ExecuteCallback =
      base::OnceCallback<void(absl::optional<double> result)>;

  // Sends a frame of heatmap data to ML service and calls the provided callback
  // method.
  void Execute(const std::vector<double>& data, ExecuteCallback callback);

 private:
  void LazyInitialize();
  void OnExecuteDone(
      ExecuteCallback callback,
      chromeos::machine_learning::mojom::ExecuteResult result,
      absl::optional<std::vector<chromeos::machine_learning::mojom::TensorPtr>>
          outputs);
  void OnLoadModel(chromeos::machine_learning::mojom::LoadModelResult result);
  void OnConnectionError();

  mojo::Remote<chromeos::machine_learning::mojom::Model> model_;
  mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor> executor_;
  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;

  base::WeakPtrFactory<HeatmapMlAgent> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_ML_AGENT_H_
