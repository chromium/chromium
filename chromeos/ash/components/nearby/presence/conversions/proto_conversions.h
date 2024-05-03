// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_

#include <optional>

#include "chromeos/ash/components/nearby/common/proto/timestamp.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom-forward.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_callbacks.h"

namespace ash::nearby::presence::proto {

::nearby::internal::DeviceIdentityMetaData BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& device_name,
    const std::string& mac_address,
    const std::string& device_id);

mojom::PresenceDeviceType DeviceTypeToMojom(
    ::nearby::internal::DeviceType device_type);
mojom::PublicCredentialType PublicCredentialTypeToMojom(
    ::nearby::presence::PublicCredentialType public_credential_type);
mojom::MetadataPtr MetadataToMojom(
    ::nearby::internal::DeviceIdentityMetaData metadata);
mojom::PrivateKeyPtr PrivateKeyToMojom(
    ::nearby::internal::LocalCredential::PrivateKey private_key);

::nearby::internal::IdentityType IdentityTypeFromMojom(
    mojom::IdentityType identity_type);
::nearby::internal::LocalCredential::PrivateKey PrivateKeyFromMojom(
    mojom::PrivateKey* private_key);
::nearby::internal::CredentialType CredentialTypeFromMojom(
    mojom::CredentialType credential_type);
::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential);
::nearby::internal::LocalCredential LocalCredentialFromMojom(
    mojom::LocalCredential* local_credential);

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type);
mojom::CredentialType CredentialTypeToMojom(
    ::nearby::internal::CredentialType credential_type);
mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential);
mojom::LocalCredentialPtr LocalCredentialToMojom(
    ::nearby::internal::LocalCredential local_credential);

ash::nearby::proto::PublicCertificate PublicCertificateFromSharedCredential(
    ::nearby::internal::SharedCredential shared_credential);
ash::nearby::proto::TrustType TrustTypeFromIdentityType(
    ::nearby::internal::IdentityType identity_type);
int64_t MillisecondsToSeconds(int64_t milliseconds);

::nearby::internal::IdentityType TrustTypeToIdentityType(
    ash::nearby::proto::TrustType trust_type);

::nearby::internal::SharedCredential
RemoteSharedCredentialToThirdPartySharedCredential(
    ash::nearby::proto::SharedCredential remote_shared_credential);

}  // namespace ash::nearby::presence::proto

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_
