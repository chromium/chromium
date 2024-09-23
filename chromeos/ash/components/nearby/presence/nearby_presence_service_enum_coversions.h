// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_

#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType ConvertMojomDeviceType(
    mojom::PresenceDeviceType mojom_type);

NearbyPresenceService::PresenceIdentityType ConvertToMojomIdentityType(
    ::nearby::internal::IdentityType identity_type_);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_
