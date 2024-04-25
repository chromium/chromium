// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_PREF_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_PREF_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::kiosk_vision {

// Observes prefs relevant for Kiosk Vision and runs the given callbacks as
// prefs change.
class PrefObserver {
 public:
  PrefObserver(PrefService* pref_service,
               base::RepeatingClosure on_enabled,
               base::RepeatingClosure on_disabled);
  PrefObserver(const PrefObserver&) = delete;
  PrefObserver& operator=(const PrefObserver&) = delete;
  ~PrefObserver();

  // Runs one of `on_enabled_` or `on_disabled_` based on the current pref
  // state, and starts observing pref changes for future notifications.
  void Start();

 private:
  void OnKioskVisionPrefChanged();

  base::RepeatingClosure on_enabled_;
  base::RepeatingClosure on_disabled_;

  PrefChangeRegistrar registrar_;
};

bool IsTelemetryPrefEnabled(const PrefService& pref_service);

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_PREF_OBSERVER_H_
