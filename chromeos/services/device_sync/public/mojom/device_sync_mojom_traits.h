// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_

#include "chromeos/services/device_sync/feature_status_change.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<chromeos::device_sync::mojom::ConnectivityStatus,
                 cryptauthv2::ConnectivityStatus> {
 public:
  static chromeos::device_sync::mojom::ConnectivityStatus ToMojom(
      cryptauthv2::ConnectivityStatus input);
  static bool FromMojom(chromeos::device_sync::mojom::ConnectivityStatus input,
                        cryptauthv2::ConnectivityStatus* out);
};

template <>
class EnumTraits<chromeos::device_sync::mojom::FeatureStatusChange,
                 chromeos::device_sync::FeatureStatusChange> {
 public:
  static chromeos::device_sync::mojom::FeatureStatusChange ToMojom(
      chromeos::device_sync::FeatureStatusChange input);
  static bool FromMojom(chromeos::device_sync::mojom::FeatureStatusChange input,
                        chromeos::device_sync::FeatureStatusChange* out);
};

template <>
class EnumTraits<chromeos::device_sync::mojom::CryptAuthService,
                 cryptauthv2::TargetService> {
 public:
  static chromeos::device_sync::mojom::CryptAuthService ToMojom(
      cryptauthv2::TargetService input);
  static bool FromMojom(chromeos::device_sync::mojom::CryptAuthService input,
                        cryptauthv2::TargetService* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
