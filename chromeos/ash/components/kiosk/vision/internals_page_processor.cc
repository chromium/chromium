// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash::kiosk_vision {

namespace {

mojom::StatePtr NewState(mojom::Status status,
                         std::vector<mojom::BoxPtr> boxes = {}) {
  return mojom::State::New(status, std::move(boxes));
}

mojom::BoxPtr ToPageBox(const cros::mojom::KioskVisionBoundingBox& box) {
  return mojom::Box::New(/*x=*/box.x,
                         /*y=*/box.y,
                         /*width=*/box.width,
                         /*height=*/box.height);
}

std::vector<mojom::BoxPtr> DetectionToBoxes(
    const cros::mojom::KioskVisionDetection& detection) {
  std::vector<mojom::BoxPtr> boxes;
  for (const auto& appearance : detection.appearances) {
    if (appearance->face) {
      boxes.push_back(ToPageBox(*appearance->face->box));
    }
    if (appearance->body) {
      boxes.push_back(ToPageBox(*appearance->body->box));
    }
  }
  return boxes;
}

}  // namespace

InternalsPageProcessor::InternalsPageProcessor()
    : last_state_(NewState(mojom::Status::kUnknown)) {}

InternalsPageProcessor::~InternalsPageProcessor() = default;

void InternalsPageProcessor::OnFrameProcessed(
    const cros::mojom::KioskVisionDetection& detection) {
  NotifyStateChange(
      NewState(mojom::Status::kRunning, DetectionToBoxes(detection)));
}

void InternalsPageProcessor::OnError(cros::mojom::KioskVisionError error) {
  NotifyStateChange(NewState(mojom::Status::kError));
}

void InternalsPageProcessor::NotifyStateChange(mojom::StatePtr new_state) {
  last_state_ = std::move(new_state);
  for (auto& observer : observers_) {
    observer.OnStateChange(*last_state_);
  }
}

void InternalsPageProcessor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->OnStateChange(*last_state_);
}

void InternalsPageProcessor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BASE_FEATURE(kEnableKioskVisionInternalsPage,
             "EnableKioskVisionInternalsPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsInternalsPageEnabled() {
  return base::FeatureList::IsEnabled(kEnableKioskVisionInternalsPage);
}

}  // namespace ash::kiosk_vision
