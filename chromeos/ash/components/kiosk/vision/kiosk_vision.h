// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_

#include "base/component_export.h"
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
  KioskVision(const KioskVision&) = delete;
  KioskVision& operator=(const KioskVision&) = delete;
  ~KioskVision();

 private:
  void Enable();
  void Disable();

  PrefObserver pref_observer_;
};

inline constexpr char kKioskVisionDlcId[] = "kiosk-vision";

// Registers prefs used in Kiosk Vision.
COMPONENT_EXPORT(KIOSK_VISION)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_KIOSK_VISION_H_
