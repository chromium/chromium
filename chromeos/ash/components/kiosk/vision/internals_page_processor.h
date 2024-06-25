// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNALS_PAGE_PROCESSOR_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNALS_PAGE_PROCESSOR_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom-forward.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"

namespace ash::kiosk_vision {

// Forwards detections to instances of the chrome://kiosk-vision-internals page
// `UIController` via the `Observer` interface.
//
// Used to aid in evaluating the Kiosk Vision feature during development.
class COMPONENT_EXPORT(KIOSK_VISION) InternalsPageProcessor
    : public DetectionProcessor {
 public:
  InternalsPageProcessor();
  InternalsPageProcessor(const InternalsPageProcessor&) = delete;
  InternalsPageProcessor& operator=(const InternalsPageProcessor&) = delete;
  ~InternalsPageProcessor() override;

  // Observer for `State` updates. Intended to be implemented under the
  // `UIController` for chrome://kiosk-vision-internals.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStateChange(const mojom::State& new_state) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // `DetectionProcessor` implementation.
  void OnFrameProcessed(
      const cros::mojom::KioskVisionDetection& detection) override;
  void OnTrackCompleted(const cros::mojom::KioskVisionTrack& track) override;
  void OnError(cros::mojom::KioskVisionError error) override;

  // Calls `Observer::OnStateChange` with `new_state` on all `observers_`.
  void NotifyStateChange(mojom::StatePtr new_state);

  // The last state emitted to `Observer::OnStateChange`.
  mojom::StatePtr last_state_;

  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observers_;
};

COMPONENT_EXPORT(KIOSK_VISION)
BASE_DECLARE_FEATURE(kEnableKioskVisionInternalsPage);

bool COMPONENT_EXPORT(KIOSK_VISION) IsInternalsPageEnabled();

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNALS_PAGE_PROCESSOR_H_
