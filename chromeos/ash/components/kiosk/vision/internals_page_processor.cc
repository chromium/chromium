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
                         std::vector<mojom::LabelPtr> labels = {},
                         std::vector<mojom::BoxPtr> boxes = {},
                         std::vector<mojom::FacePtr> faces = {}) {
  return mojom::State::New(status, std::move(labels), std::move(boxes),
                           std::move(faces));
}

mojom::LabelPtr ToPageLabel(int person_id,
                            const cros::mojom::KioskVisionBoundingBox& box) {
  return mojom::Label::New(person_id, box.x, box.y);
}

std::vector<mojom::LabelPtr> ToLabels(
    const std::vector<cros::mojom::KioskVisionAppearancePtr>& appearances) {
  std::vector<mojom::LabelPtr> labels;
  for (const auto& appearance : appearances) {
    // An appearance can have two boxes. Add a label for one of the two.
    if (appearance->body) {
      labels.push_back(
          ToPageLabel(appearance->person_id, *appearance->body->box));
    } else if (appearance->face) {
      labels.push_back(
          ToPageLabel(appearance->person_id, *appearance->face->box));
    }
  }
  return labels;
}

mojom::BoxPtr ToPageBox(const cros::mojom::KioskVisionBoundingBox& box) {
  return mojom::Box::New(/*x=*/box.x,
                         /*y=*/box.y,
                         /*width=*/box.width,
                         /*height=*/box.height);
}

std::vector<mojom::BoxPtr> ToBoxes(
    const std::vector<cros::mojom::KioskVisionAppearancePtr>& appearances) {
  std::vector<mojom::BoxPtr> boxes;
  for (const auto& appearance : appearances) {
    if (appearance->face) {
      boxes.push_back(ToPageBox(*appearance->face->box));
    }
    if (appearance->body) {
      boxes.push_back(ToPageBox(*appearance->body->box));
    }
  }
  return boxes;
}

mojom::FacePtr ToPageFace(const cros::mojom::KioskVisionFaceDetection& face) {
  return mojom::Face::New(
      /*roll=*/face.roll,
      /*pan=*/face.pan,
      /*tilt=*/face.tilt,
      /*box=*/ToPageBox(*face.box));
}

std::vector<mojom::FacePtr> ToFaces(
    const std::vector<cros::mojom::KioskVisionAppearancePtr>& appearances) {
  std::vector<mojom::FacePtr> faces;
  for (const auto& appearance : appearances) {
    if (appearance->face) {
      faces.push_back(ToPageFace(*appearance->face));
    }
  }
  return faces;
}

}  // namespace

InternalsPageProcessor::InternalsPageProcessor()
    : last_state_(NewState(mojom::Status::kUnknown)) {}

InternalsPageProcessor::~InternalsPageProcessor() = default;

void InternalsPageProcessor::OnFrameProcessed(
    const cros::mojom::KioskVisionDetection& detection) {
  NotifyStateChange(
      NewState(mojom::Status::kRunning, ToLabels(detection.appearances),
               ToBoxes(detection.appearances), ToFaces(detection.appearances)));
}

void InternalsPageProcessor::OnTrackCompleted(
    const cros::mojom::KioskVisionTrack& track) {
  NotifyStateChange(NewState(mojom::Status::kRunning,
                             ToLabels(track.appearances),
                             ToBoxes(track.appearances)));
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
