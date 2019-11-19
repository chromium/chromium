// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/mojom/device_sync_mojom_traits.h"

namespace mojo {

chromeos::device_sync::mojom::ConnectivityStatus EnumTraits<
    chromeos::device_sync::mojom::ConnectivityStatus,
    cryptauthv2::ConnectivityStatus>::ToMojom(cryptauthv2::ConnectivityStatus
                                                  input) {
  switch (input) {
    case cryptauthv2::ConnectivityStatus::ONLINE:
      return chromeos::device_sync::mojom::ConnectivityStatus::kOnline;
    case cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY:
      return chromeos::device_sync::mojom::ConnectivityStatus::
          kUnknownConnectivity;
    case cryptauthv2::ConnectivityStatus::OFFLINE:
      return chromeos::device_sync::mojom::ConnectivityStatus::kOffline;
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
    case cryptauthv2::ConnectivityStatus::
        ConnectivityStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return chromeos::device_sync::mojom::ConnectivityStatus::
          kUnknownConnectivity;
  }
}

bool EnumTraits<chromeos::device_sync::mojom::ConnectivityStatus,
                cryptauthv2::ConnectivityStatus>::
    FromMojom(chromeos::device_sync::mojom::ConnectivityStatus input,
              cryptauthv2::ConnectivityStatus* out) {
  switch (input) {
    case chromeos::device_sync::mojom::ConnectivityStatus::kOnline:
      *out = cryptauthv2::ConnectivityStatus::ONLINE;
      return true;
    case chromeos::device_sync::mojom::ConnectivityStatus::kOffline:
      *out = cryptauthv2::ConnectivityStatus::OFFLINE;
      return true;
    case chromeos::device_sync::mojom::ConnectivityStatus::kUnknownConnectivity:
      *out = cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY;
      return true;
  }

  NOTREACHED();
  return false;
}

chromeos::device_sync::mojom::FeatureStatusChange
EnumTraits<chromeos::device_sync::mojom::FeatureStatusChange,
           chromeos::device_sync::FeatureStatusChange>::
    ToMojom(chromeos::device_sync::FeatureStatusChange input) {
  switch (input) {
    case chromeos::device_sync::FeatureStatusChange::kEnableExclusively:
      return chromeos::device_sync::mojom::FeatureStatusChange::
          kEnableExclusively;
    case chromeos::device_sync::FeatureStatusChange::kEnableNonExclusively:
      return chromeos::device_sync::mojom::FeatureStatusChange::
          kEnableNonExclusively;
    case chromeos::device_sync::FeatureStatusChange::kDisable:
      return chromeos::device_sync::mojom::FeatureStatusChange::kDisable;
  }
}

bool EnumTraits<chromeos::device_sync::mojom::FeatureStatusChange,
                chromeos::device_sync::FeatureStatusChange>::
    FromMojom(chromeos::device_sync::mojom::FeatureStatusChange input,
              chromeos::device_sync::FeatureStatusChange* out) {
  switch (input) {
    case chromeos::device_sync::mojom::FeatureStatusChange::kEnableExclusively:
      *out = chromeos::device_sync::FeatureStatusChange::kEnableExclusively;
      return true;
    case chromeos::device_sync::mojom::FeatureStatusChange::
        kEnableNonExclusively:
      *out = chromeos::device_sync::FeatureStatusChange::kEnableNonExclusively;
      return true;
    case chromeos::device_sync::mojom::FeatureStatusChange::kDisable:
      *out = chromeos::device_sync::FeatureStatusChange::kDisable;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
