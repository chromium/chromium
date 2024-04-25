// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash::kiosk_vision {

PrefObserver::PrefObserver(PrefService* pref_service,
                           base::RepeatingClosure on_enabled,
                           base::RepeatingClosure on_disabled)
    : on_enabled_(std::move(on_enabled)), on_disabled_(std::move(on_disabled)) {
  registrar_.Init(pref_service);
}

PrefObserver::~PrefObserver() = default;

void PrefObserver::Start() {
  registrar_.Add(prefs::kKioskVisionTelemetryEnabled,
                 base::BindRepeating(&PrefObserver::OnKioskVisionPrefChanged,
                                     base::Unretained(this)));
  OnKioskVisionPrefChanged();
}

void PrefObserver::OnKioskVisionPrefChanged() {
  IsTelemetryPrefEnabled(CHECK_DEREF(registrar_.prefs())) ? on_enabled_.Run()
                                                          : on_disabled_.Run();
}

bool IsTelemetryPrefEnabled(const PrefService& pref_service) {
  return pref_service.GetBoolean(prefs::kKioskVisionTelemetryEnabled);
}

}  // namespace ash::kiosk_vision
