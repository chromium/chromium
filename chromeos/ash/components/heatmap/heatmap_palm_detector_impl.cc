// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/heatmap/heatmap_palm_detector_impl.h"

#include "base/strings/strcat.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

namespace ash {

using ::chromeos::machine_learning::mojom::HeatmapPalmRejectionConfig;
using ::chromeos::machine_learning::mojom::HeatmapProcessedEventPtr;
using ::chromeos::machine_learning::mojom::LoadHeatmapPalmRejectionResult;

namespace {

constexpr char kSystemModelDir[] = "/opt/google/chrome/ml_models/";
constexpr base::TimeDelta kTimestampDiffThreshold = base::Milliseconds(20);
constexpr base::TimeDelta kReconnectInitialDelay = base::Seconds(1);
constexpr base::TimeDelta kReconnectMaxDelay = base::Minutes(10);

struct HeatmapModelMetadata {
  std::string model_file;
  int input_node;
  int output_node;
  double palm_threshold;
};

using MetadataMap =
    std::map<HeatmapPalmDetectorImpl::ModelId, HeatmapModelMetadata>;

// Returns a map from device ID to model metadata for each supported device.
MetadataMap GetHeatmapModelMetadata() {
  return {{HeatmapPalmDetectorImpl::ModelId::kRex,
           {
               .model_file =
                   "mlservice-model-poncho_palm_rejection-20230907-v0.tflite",
               .input_node = 0,
               .output_node = 23,
               .palm_threshold = 0.6,
           }},
          {HeatmapPalmDetectorImpl::ModelId::kGeralt,
           {
               .model_file =
                   "mlservice-model-poncho-palm_rejection_g-20240313-v0.tflite",
               .input_node = 0,
               .output_node = 21,
               .palm_threshold = 0.6,
           }}};
}

bool CanBeMatched(base::Time t1, base::Time t2) {
  return (t1 - t2).magnitude() < kTimestampDiffThreshold;
}
}  // namespace

HeatmapPalmDetectorImpl::HeatmapPalmDetectorImpl()
    : reconnect_delay_(kReconnectInitialDelay), client_(this) {}

HeatmapPalmDetectorImpl::~HeatmapPalmDetectorImpl() = default;

void HeatmapPalmDetectorImpl::Start(ModelId model_id,
                                    std::string_view hidraw_path,
                                    std::optional<CropHeatmap> crop_heatmap) {
  crop_heatmap_ = crop_heatmap;
  model_id_ = model_id;
  hidraw_path_ = hidraw_path;
  const MetadataMap model_metadata = GetHeatmapModelMetadata();
  const auto metadata_lookup = model_metadata.find(model_id);
  if (metadata_lookup == model_metadata.end()) {
    LOG(ERROR) << "Invalid model ID: " << static_cast<int>(model_id);
    return;
  }

  auto config = HeatmapPalmRejectionConfig::New();
  config->tf_model_path =
      base::StrCat({kSystemModelDir, metadata_lookup->second.model_file});
  config->input_node = metadata_lookup->second.input_node;
  config->output_node = metadata_lookup->second.output_node;
  config->palm_threshold = metadata_lookup->second.palm_threshold;
  config->heatmap_hidraw_device = hidraw_path;

  if (crop_heatmap) {
    if (!config->crop_heatmap) {
      config->crop_heatmap =
          ::chromeos::machine_learning::mojom::CropHeatmap::New();
    }
    config->crop_heatmap->bottom_crop = crop_heatmap->bottom_crop;
    config->crop_heatmap->left_crop = crop_heatmap->left_crop;
    config->crop_heatmap->right_crop = crop_heatmap->right_crop;
    config->crop_heatmap->top_crop = crop_heatmap->top_crop;
  }

  if (!ml_service_) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  }
  ml_service_.set_disconnect_handler(base::BindOnce(
      &HeatmapPalmDetectorImpl::OnConnectionError, weak_factory_.GetWeakPtr()));
  ml_service_->LoadHeatmapPalmRejection(
      std::move(config), client_.BindNewPipeAndPassRemote(),
      base::BindOnce(&HeatmapPalmDetectorImpl::OnLoadHeatmapPalmRejection,
                     weak_factory_.GetWeakPtr()));
}

void HeatmapPalmDetectorImpl::OnConnectionError() {
  ml_service_.reset();
  client_.reset();
  is_ready_ = false;
  std::queue<TouchRecord>().swap(touch_records_);
  palm_tracking_ids_.clear();

  delay_timer_.Start(
      FROM_HERE, reconnect_delay_,
      base::BindOnce(&HeatmapPalmDetectorImpl::Start,
                     weak_factory_.GetWeakPtr(), model_id_,
                     hidraw_path_, crop_heatmap_));
  if (reconnect_delay_ <
      kReconnectMaxDelay) {  // exponential backoff with max limit
    reconnect_delay_ *= 2;
  }
}

void HeatmapPalmDetectorImpl::OnLoadHeatmapPalmRejection(
    LoadHeatmapPalmRejectionResult result) {
  reconnect_delay_ = kReconnectInitialDelay;
  if (result == LoadHeatmapPalmRejectionResult::OK) {
    std::queue<TouchRecord>().swap(touch_records_);
    palm_tracking_ids_.clear();
    is_ready_ = true;
  }
}

void HeatmapPalmDetectorImpl::OnHeatmapProcessedEvent(
    HeatmapProcessedEventPtr event) {
  if (touch_records_.empty()) {
    return;
  }
  TouchRecord best_match = touch_records_.front();
  if (best_match.timestamp > event->timestamp) {
    if (CanBeMatched(best_match.timestamp, event->timestamp)) {
      touch_records_.pop();
    } else {
      // Cannot find a matching record.
      return;
    }
  } else {
    // Find the last record which is before the heatmap data.
    while (!touch_records_.empty() &&
           touch_records_.front().timestamp < event->timestamp) {
      best_match = touch_records_.front();
      touch_records_.pop();
    }
    // Check if the next record is a better match.
    if (!touch_records_.empty() &&
        touch_records_.front().timestamp - event->timestamp <
            event->timestamp - best_match.timestamp &&
        CanBeMatched(touch_records_.front().timestamp, event->timestamp)) {
      best_match = touch_records_.front();
      touch_records_.pop();
    }
    if (!CanBeMatched(best_match.timestamp, event->timestamp)) {
      // Cannot find a matching record.
      return;
    }
  }
  if (event->is_palm) {
    for (int id : best_match.tracking_ids) {
      palm_tracking_ids_.insert(id);
    }
  }
}

bool HeatmapPalmDetectorImpl::IsPalm(int tracking_id) const {
  return palm_tracking_ids_.find(tracking_id) != palm_tracking_ids_.end();
}

bool HeatmapPalmDetectorImpl::IsReady() const {
  return is_ready_;
}

void HeatmapPalmDetectorImpl::AddTouchRecord(
    base::Time timestamp,
    const std::vector<int>& tracking_ids) {
  touch_records_.push(TouchRecord(timestamp, tracking_ids));
}

void HeatmapPalmDetectorImpl::RemoveTouch(int tracking_id) {
  palm_tracking_ids_.erase(tracking_id);
}

}  // namespace ash
