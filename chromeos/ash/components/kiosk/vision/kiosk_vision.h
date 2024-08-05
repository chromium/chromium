// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/internal/retry_timer.h"
#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"
#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::kiosk_vision {

// Manages the hierarchy of objects involved in the Kiosk Vision ML feature.
//
// There are two consumers of this feature: the backend telemetry API, and the
// web app API. Each consumer can be enabled independently via prefs.
//
// When one or more consumers are enabled, this class communicates with the CrOS
// Camera Service to retrieve detections from our ML model. Detections are then
// processed, and forwarded to enabled consumers.
//
// TODO(b/320669284): Implement telemetry API consumer.
// TODO(b/319090475): Implement web app API consumer.
class COMPONENT_EXPORT(KIOSK_VISION) KioskVision {
 public:
  explicit KioskVision(PrefService* pref_service);
  KioskVision(const KioskVision&) = delete;
  KioskVision& operator=(const KioskVision&) = delete;
  ~KioskVision();

  // Returns the telemetry processor. `nullptr` if it is disabled.
  TelemetryProcessor* GetTelemetryProcessor();

  // Returns the chrome://kiosk-vision-internals processor. `nullptr` if it is
  // disabled.
  InternalsPageProcessor* GetInternalsPageProcessor();

  const CameraServiceConnector* GetCameraConnectorForTesting() const;

 private:
  void Enable();
  void Disable();

  // Sets up enabled processors and connects them to camera service detections.
  void InitializeProcessors(std::string dlc_path);

  void OnDlcInstallError();

  // `nullopt` if the telemetry API consumer is disabled.
  std::optional<TelemetryProcessor> telemetry_processor_;

  // `nullopt` if the internals page is disabled.
  std::optional<InternalsPageProcessor> internals_webui_processor_;

  // `nullopt` if this feature is disabled.
  std::optional<DetectionObserver> detection_observer_;

  // `nullopt` if this feature is disabled.
  std::optional<CameraServiceConnector> camera_connector_;

  PrefObserver pref_observer_;

  RetryTimer retry_timer_;

  // `base::WeakPtrFactory` must be the last field so it's destroyed first.
  base::WeakPtrFactory<KioskVision> weak_ptr_factory_{this};
};

inline constexpr char kKioskVisionDlcId[] = "cros-camera-kiosk-vision-dlc";

// Registers prefs used in Kiosk Vision.
COMPONENT_EXPORT(KIOSK_VISION)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
