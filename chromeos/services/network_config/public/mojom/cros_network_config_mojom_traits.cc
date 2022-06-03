// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/mojom/cros_network_config_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

chromeos::network_config::mojom::ProxyMode
EnumTraits<chromeos::network_config::mojom::ProxyMode,
           ProxyPrefs::ProxyMode>::ToMojom(ProxyPrefs::ProxyMode input) {
  switch (input) {
    case ProxyPrefs::MODE_DIRECT:
      return chromeos::network_config::mojom::ProxyMode::kDirect;
    case ProxyPrefs::MODE_AUTO_DETECT:
      return chromeos::network_config::mojom::ProxyMode::kAutoDetect;
    case ProxyPrefs::MODE_PAC_SCRIPT:
      return chromeos::network_config::mojom::ProxyMode::kPacScript;
    case ProxyPrefs::MODE_FIXED_SERVERS:
      return chromeos::network_config::mojom::ProxyMode::kFixedServers;
    case ProxyPrefs::MODE_SYSTEM:
      return chromeos::network_config::mojom::ProxyMode::kSystem;
    case ProxyPrefs::kModeCount:
      break;
  }

  NOTREACHED();
  return chromeos::network_config::mojom::ProxyMode::kDirect;
}

bool EnumTraits<chromeos::network_config::mojom::ProxyMode,
                ProxyPrefs::ProxyMode>::
    FromMojom(chromeos::network_config::mojom::ProxyMode input,
              ProxyPrefs::ProxyMode* out) {
  switch (input) {
    case chromeos::network_config::mojom::ProxyMode::kDirect:
      *out = ProxyPrefs::MODE_DIRECT;
      return true;
    case chromeos::network_config::mojom::ProxyMode::kAutoDetect:
      *out = ProxyPrefs::MODE_AUTO_DETECT;
      return true;
    case chromeos::network_config::mojom::ProxyMode::kPacScript:
      *out = ProxyPrefs::MODE_PAC_SCRIPT;
      return true;
    case chromeos::network_config::mojom::ProxyMode::kFixedServers:
      *out = ProxyPrefs::MODE_FIXED_SERVERS;
      return true;
    case chromeos::network_config::mojom::ProxyMode::kSystem:
      *out = ProxyPrefs::MODE_SYSTEM;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
