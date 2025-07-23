// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"

#include <cstddef>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash::kiosk_vision {

InternalsPageProcessor::InternalsPageProcessor() = default;

InternalsPageProcessor::~InternalsPageProcessor() = default;

void InternalsPageProcessor::OnFrameProcessed(
    const cros::mojom::KioskVisionDetection& detection) {
  NOTIMPLEMENTED();
}

void InternalsPageProcessor::OnTrackCompleted(
    const cros::mojom::KioskVisionTrack& track) {
  NOTIMPLEMENTED();
}

void InternalsPageProcessor::OnError(cros::mojom::KioskVisionError error) {
  NOTIMPLEMENTED();
}

void InternalsPageProcessor::NotifyStateChange() {
  NOTIMPLEMENTED();
}

void InternalsPageProcessor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
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
