// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_CONVERSIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_CONVERSIONS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom-forward.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType DeviceTypeFromMojom(
    mojom::PresenceDeviceType device_type);
::nearby::internal::Metadata MetadataFromMojom(mojom::Metadata* metadata);

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type);
mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential);

}  // namespace ash::nearby::presence

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_CONVERSIONS_H_
