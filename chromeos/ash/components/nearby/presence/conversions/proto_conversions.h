// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_

#include "chromeos/ash/components/nearby/common/proto/timestamp.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

namespace ash::nearby::presence::proto {

::nearby::internal::Metadata BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& account_name,
    const std::string& device_name,
    const std::string& user_name,
    const std::string& profile_url,
    const std::string& mac_address);

mojom::PresenceDeviceType DeviceTypeToMojom(
    ::nearby::internal::DeviceType device_type);
mojom::MetadataPtr MetadataToMojom(::nearby::internal::Metadata metadata);
mojom::PrivateKeyPtr PrivateKeyToMojom(
    ::nearby::internal::LocalCredential::PrivateKey private_key);

::nearby::internal::IdentityType IdentityTypeFromMojom(
    mojom::IdentityType identity_type);
::nearby::internal::LocalCredential::PrivateKey PrivateKeyFromMojom(
    mojom::PrivateKey* private_key);
::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential);
::nearby::internal::LocalCredential LocalCredentialFromMojom(
    mojom::LocalCredential* local_credential);

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type);
mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential);
mojom::LocalCredentialPtr LocalCredentialToMojom(
    ::nearby::internal::LocalCredential local_credential);

ash::nearby::proto::PublicCertificate PublicCertificateFromSharedCredential(
    ::nearby::internal::SharedCredential shared_credential);
ash::nearby::proto::TrustType TrustTypeFromIdentityType(
    ::nearby::internal::IdentityType identity_type);
int64_t MillisecondsToSeconds(int64_t milliseconds);

::nearby::internal::SharedCredential PublicCertificateToSharedCredential(
    ash::nearby::proto::PublicCertificate certificate);
::nearby::internal::IdentityType TrustTypeToIdentityType(
    ash::nearby::proto::TrustType trust_type);
int64_t SecondsToMilliseconds(int64_t seconds);

}  // namespace ash::nearby::presence::proto

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CONVERSIONS_PROTO_CONVERSIONS_H_
