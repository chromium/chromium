// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_NEARBY_PRESENCE_CONVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_NEARBY_PRESENCE_CONVERSIONS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom-forward.h"
#include "mojo/public/mojom/base/absl_status.mojom-forward.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"
#include "third_party/nearby/src/presence/presence_device.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType DeviceTypeFromMojom(
    mojom::PresenceDeviceType device_type);
mojom::PresenceDeviceType DeviceTypeToMojom(
    ::nearby::internal::DeviceType device_type);

::nearby::internal::DeviceIdentityMetaData MetadataFromMojom(
    mojom::Metadata* metadata);
mojom::MetadataPtr MetadataToMojom(
    ::nearby::internal::DeviceIdentityMetaData metadata);

mojom::IdentityType ConvertIdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type);
::nearby::internal::IdentityType ConvertMojomIdentityType(
    mojom::IdentityType identity_type);

mojom::CredentialType ConvertCredentialTypeToMojom(
    ::nearby::internal::CredentialType credential_type);
::nearby::internal::CredentialType ConvertMojomCredentialType(
    mojom::CredentialType credential_type);

mojom::ActionType ConvertActionTypeToMojom(uint32_t action);
mojo_base::mojom::AbslStatusCode ConvertStatusToMojom(absl::Status status);

mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential);
::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential);

mojom::PresenceDevicePtr BuildPresenceMojomDevice(
    ::nearby::presence::PresenceDevice device);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_NEARBY_PRESENCE_CONVERSIONS_H_
