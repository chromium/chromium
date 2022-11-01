// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/mojom/device_sync_mojom_traits.h"

namespace mojo {

ash::device_sync::mojom::ConnectivityStatus EnumTraits<
    ash::device_sync::mojom::ConnectivityStatus,
    cryptauthv2::ConnectivityStatus>::ToMojom(cryptauthv2::ConnectivityStatus
                                                  input) {
  switch (input) {
    case cryptauthv2::ConnectivityStatus::ONLINE:
      return ash::device_sync::mojom::ConnectivityStatus::kOnline;
    case cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY:
      return ash::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity;
    case cryptauthv2::ConnectivityStatus::OFFLINE:
      return ash::device_sync::mojom::ConnectivityStatus::kOffline;
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return ash::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity;
  }
}

bool EnumTraits<ash::device_sync::mojom::ConnectivityStatus,
                cryptauthv2::ConnectivityStatus>::
    FromMojom(ash::device_sync::mojom::ConnectivityStatus input,
              cryptauthv2::ConnectivityStatus* out) {
  switch (input) {
    case ash::device_sync::mojom::ConnectivityStatus::kOnline:
      *out = cryptauthv2::ConnectivityStatus::ONLINE;
      return true;
    case ash::device_sync::mojom::ConnectivityStatus::kOffline:
      *out = cryptauthv2::ConnectivityStatus::OFFLINE;
      return true;
    case ash::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity:
      *out = cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY;
      return true;
  }

  NOTREACHED();
  return false;
}

ash::device_sync::mojom::FeatureStatusChange
EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
           ash::device_sync::FeatureStatusChange>::
    ToMojom(ash::device_sync::FeatureStatusChange input) {
  switch (input) {
    case ash::device_sync::FeatureStatusChange::kEnableExclusively:
      return ash::device_sync::mojom::FeatureStatusChange::kEnableExclusively;
    case ash::device_sync::FeatureStatusChange::kEnableNonExclusively:
      return ash::device_sync::mojom::FeatureStatusChange::
          kEnableNonExclusively;
    case ash::device_sync::FeatureStatusChange::kDisable:
      return ash::device_sync::mojom::FeatureStatusChange::kDisable;
  }
}

bool EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
                ash::device_sync::FeatureStatusChange>::
    FromMojom(ash::device_sync::mojom::FeatureStatusChange input,
              ash::device_sync::FeatureStatusChange* out) {
  switch (input) {
    case ash::device_sync::mojom::FeatureStatusChange::kEnableExclusively:
      *out = ash::device_sync::FeatureStatusChange::kEnableExclusively;
      return true;
    case ash::device_sync::mojom::FeatureStatusChange::kEnableNonExclusively:
      *out = ash::device_sync::FeatureStatusChange::kEnableNonExclusively;
      return true;
    case ash::device_sync::mojom::FeatureStatusChange::kDisable:
      *out = ash::device_sync::FeatureStatusChange::kDisable;
      return true;
  }

  NOTREACHED();
  return false;
}

ash::device_sync::mojom::CryptAuthService EnumTraits<
    ash::device_sync::mojom::CryptAuthService,
    cryptauthv2::TargetService>::ToMojom(cryptauthv2::TargetService input) {
  switch (input) {
    case cryptauthv2::TargetService::ENROLLMENT:
      return ash::device_sync::mojom::CryptAuthService::kEnrollment;
    case cryptauthv2::TargetService::DEVICE_SYNC:
      return ash::device_sync::mojom::CryptAuthService::kDeviceSync;
    case cryptauthv2::TargetService::TARGET_SERVICE_UNSPECIFIED:
      [[fallthrough]];
    case cryptauthv2::TargetService::TargetService_INT_MIN_SENTINEL_DO_NOT_USE_:
      [[fallthrough]];
    case cryptauthv2::TargetService::TargetService_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return ash::device_sync::mojom::CryptAuthService::kDeviceSync;
  }
}

bool EnumTraits<ash::device_sync::mojom::CryptAuthService,
                cryptauthv2::TargetService>::
    FromMojom(ash::device_sync::mojom::CryptAuthService input,
              cryptauthv2::TargetService* out) {
  switch (input) {
    case ash::device_sync::mojom::CryptAuthService::kEnrollment:
      *out = cryptauthv2::TargetService::ENROLLMENT;
      return true;
    case ash::device_sync::mojom::CryptAuthService::kDeviceSync:
      *out = cryptauthv2::TargetService::DEVICE_SYNC;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
