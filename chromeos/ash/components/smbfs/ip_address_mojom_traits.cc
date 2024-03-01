// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/ip_address_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<smbfs::mojom::IPAddressDataView, net::IPAddress>::Read(
    smbfs::mojom::IPAddressDataView data,
    net::IPAddress* out) {
  std::vector<uint8_t> bytes;
  if (!data.ReadAddressBytes(&bytes))
    return false;

  if (bytes.size() && bytes.size() != net::IPAddress::kIPv4AddressSize &&
      bytes.size() != net::IPAddress::kIPv6AddressSize) {
    return false;
  }

  *out = net::IPAddress(bytes);
  return true;
}

}  // namespace mojo
