// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_CONTEXT_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_CONTEXT_H_

#include "chromeos/ash/components/network/network_util.h"

namespace ash::geolocation {

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    GeopositionContext {
  // The list of observed Wi-Fi access points used for the geocoding
  // request.
  WifiAccessPointVector wifi_context;
  // The list of observed cell towers used for the geocoding request.
  CellTowerVector cell_tower_context;

  GeopositionContext();

  // Move-only.
  GeopositionContext(const GeopositionContext&) = delete;
  GeopositionContext& operator=(const GeopositionContext&) = delete;
  GeopositionContext(GeopositionContext&&);
  GeopositionContext& operator=(GeopositionContext&&);

  ~GeopositionContext();
};

}  // namespace ash::geolocation

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_CONTEXT_H_
