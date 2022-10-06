// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "chromeos/ash/components/smbfs/mojom/ip_address.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/ip_address.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<smbfs::mojom::IPAddressDataView, net::IPAddress> {
  static base::span<const uint8_t> address_bytes(
      const net::IPAddress& ip_address) {
    return ip_address.bytes();
  }

  static bool Read(smbfs::mojom::IPAddressDataView obj, net::IPAddress* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_
