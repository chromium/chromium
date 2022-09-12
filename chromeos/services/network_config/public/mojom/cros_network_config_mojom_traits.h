// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_MOJOM_CROS_NETWORK_CONFIG_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_MOJOM_CROS_NETWORK_CONFIG_MOJOM_TRAITS_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "components/proxy_config/proxy_prefs.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<chromeos::network_config::mojom::ProxyMode,
                 ProxyPrefs::ProxyMode> {
 public:
  static chromeos::network_config::mojom::ProxyMode ToMojom(
      ProxyPrefs::ProxyMode input);
  static bool FromMojom(chromeos::network_config::mojom::ProxyMode input,
                        ProxyPrefs::ProxyMode* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_MOJOM_CROS_NETWORK_CONFIG_MOJOM_TRAITS_H_
