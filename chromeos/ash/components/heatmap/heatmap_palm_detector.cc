// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/heatmap/heatmap_palm_detector.h"

#include "base/strings/strcat.h"

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

namespace ash {

using ::chromeos::machine_learning::mojom::HeatmapPalmRejectionConfig;
using ::chromeos::machine_learning::mojom::HeatmapProcessedEventPtr;
using ::chromeos::machine_learning::mojom::LoadHeatmapPalmRejectionResult;

namespace {

constexpr char kSystemModelDir[] = "/opt/google/chrome/ml_models/";

struct HeatmapModelMetadata {
  std::string model_file;
  int input_node;
  int output_node;
  double palm_threshold;
};

using MetadataMap =
    std::map<HeatmapPalmDetector::DeviceId, HeatmapModelMetadata>;

// Returns a map from device ID to model metadata for each supported device.
MetadataMap GetHeatmapModelMetadata() {
  return {{HeatmapPalmDetector::DeviceId::kRex,
           {
               .model_file =
                   "mlservice-model-poncho_palm_rejection-20230907-v0.tflite",
               .input_node = 0,
               .output_node = 23,
               .palm_threshold = 0.6,
           }}};
}
}  // namespace

HeatmapPalmDetector::HeatmapPalmDetector() : client_(this) {}

HeatmapPalmDetector::~HeatmapPalmDetector() = default;

void HeatmapPalmDetector::Start(DeviceId device, std::string_view path) {
  const MetadataMap model_metadata = GetHeatmapModelMetadata();
  const auto metadata_lookup = model_metadata.find(device);
  if (metadata_lookup == model_metadata.end()) {
    LOG(ERROR) << "Invalid device ID";
    return;
  }

  auto config = HeatmapPalmRejectionConfig::New();
  config->tf_model_path =
      base::StrCat({kSystemModelDir, metadata_lookup->second.model_file});
  config->input_node = metadata_lookup->second.input_node;
  config->output_node = metadata_lookup->second.output_node;
  config->palm_threshold = metadata_lookup->second.palm_threshold;
  config->heatmap_hidraw_device = path;

  if (!ml_service_) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  }
  ml_service_.set_disconnect_handler(base::BindOnce(
      &HeatmapPalmDetector::OnConnectionError, weak_factory_.GetWeakPtr()));
  ml_service_->LoadHeatmapPalmRejection(
      std::move(config), client_.BindNewPipeAndPassRemote(),
      base::BindOnce(&HeatmapPalmDetector::OnLoadHeatmapPalmRejection,
                     weak_factory_.GetWeakPtr()));
}

void HeatmapPalmDetector::OnConnectionError() {
  ml_service_.reset();
  client_.reset();
  is_ready_ = false;
  is_palm_ = false;
}

void HeatmapPalmDetector::OnLoadHeatmapPalmRejection(
    LoadHeatmapPalmRejectionResult result) {
  if (result == LoadHeatmapPalmRejectionResult::OK) {
    is_ready_ = true;
  }
}

void HeatmapPalmDetector::OnHeatmapProcessedEvent(
    HeatmapProcessedEventPtr event) {
  is_palm_ = event->is_palm;
}

HeatmapPalmDetector::DetectionResult HeatmapPalmDetector::GetDetectionResult()
    const {
  return is_palm_ ? DetectionResult::kPalm : DetectionResult::kNoPalm;
}

}  // namespace ash
