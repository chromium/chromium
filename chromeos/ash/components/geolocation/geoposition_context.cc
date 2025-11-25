// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/geoposition_context.h"

namespace ash::geolocation {

GeopositionContext::GeopositionContext() = default;
GeopositionContext::GeopositionContext(GeopositionContext&&) = default;
GeopositionContext& GeopositionContext::operator=(GeopositionContext&&) =
    default;
GeopositionContext::~GeopositionContext() = default;

}  // namespace ash::geolocation
