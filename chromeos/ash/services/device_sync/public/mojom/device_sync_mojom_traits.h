// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_

#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<ash::device_sync::mojom::ConnectivityStatus,
                 cryptauthv2::ConnectivityStatus> {
 public:
  static ash::device_sync::mojom::ConnectivityStatus ToMojom(
      cryptauthv2::ConnectivityStatus input);
  static bool FromMojom(ash::device_sync::mojom::ConnectivityStatus input,
                        cryptauthv2::ConnectivityStatus* out);
};

template <>
class EnumTraits<ash::device_sync::mojom::GroupPrivateKeyStatus,
                 ash::device_sync::GroupPrivateKeyStatus> {
 public:
  static ash::device_sync::mojom::GroupPrivateKeyStatus ToMojom(
      ash::device_sync::GroupPrivateKeyStatus input);
  static bool FromMojom(ash::device_sync::mojom::GroupPrivateKeyStatus input,
                        ash::device_sync::GroupPrivateKeyStatus* out);
};

template <>
class EnumTraits<ash::device_sync::mojom::BetterTogetherMetadataStatus,
                 ash::device_sync::BetterTogetherMetadataStatus> {
 public:
  static ash::device_sync::mojom::BetterTogetherMetadataStatus ToMojom(
      ash::device_sync::BetterTogetherMetadataStatus input);
  static bool FromMojom(
      ash::device_sync::mojom::BetterTogetherMetadataStatus input,
      ash::device_sync::BetterTogetherMetadataStatus* out);
};

template <>
class EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
                 ash::device_sync::FeatureStatusChange> {
 public:
  static ash::device_sync::mojom::FeatureStatusChange ToMojom(
      ash::device_sync::FeatureStatusChange input);
  static bool FromMojom(ash::device_sync::mojom::FeatureStatusChange input,
                        ash::device_sync::FeatureStatusChange* out);
};

template <>
class EnumTraits<ash::device_sync::mojom::CryptAuthService,
                 cryptauthv2::TargetService> {
 public:
  static ash::device_sync::mojom::CryptAuthService ToMojom(
      cryptauthv2::TargetService input);
  static bool FromMojom(ash::device_sync::mojom::CryptAuthService input,
                        cryptauthv2::TargetService* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
