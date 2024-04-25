// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::kiosk_vision {

// Manages the hierarchy of objects involved in the Kiosk Vision ML feature.
//
// Its responsibilities include enabling and disabling the feature based on
// prefs; communicating with the CrOS camera service to retrieve ML model
// detections; and processing and forwarding detections to the backend telemetry
// API and the Kiosk web app.
class COMPONENT_EXPORT(KIOSK_VISION) KioskVision {
 public:
  explicit KioskVision(PrefService* pref_service);
  // Tests must make sure `test_processor` outlives `this`.
  KioskVision(PrefService* pref_service, DetectionProcessor* test_processor);
  KioskVision(const KioskVision&) = delete;
  KioskVision& operator=(const KioskVision&) = delete;
  ~KioskVision();

 private:
  void Enable();
  void Disable();

  // Sets up enabled processors and connects them to camera service detections.
  void InitializeProcessors(std::string dlc_path);

  // TODO(b/333698067): Remove once the reporting processor is implemented.
  raw_ptr<DetectionProcessor> test_processor_ = nullptr;

  // `nullopt` if this feature is disabled.
  std::optional<DetectionObserver> detection_observer_;

  // `nullopt` if this feature is disabled.
  std::optional<CameraServiceConnector> camera_connector_;

  PrefObserver pref_observer_;

  // `base::WeakPtrFactory` must be the last field so it's destroyed first.
  base::WeakPtrFactory<KioskVision> weak_ptr_factory_{this};
};

inline constexpr std::string_view kKioskVisionDlcId =
    "cros-camera-kiosk-vision-dlc";

// Registers prefs used in Kiosk Vision.
COMPONENT_EXPORT(KIOSK_VISION)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
