// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_METRICS_UTIL_H_
#define CHROMECAST_METRICS_METRICS_UTIL_H_

#include "net/base/ip_address.h"

namespace chromecast {

// Pack last two bytes of IPv4 or IPv6 address into value used for logging
// partial sender IP fragments (e.g. discovery code and virtual connection
// details). If the address is empty or not valid IPv4/IPv6 then zeros will
// be filled into the packed fragment.
uint32_t GetIPAddressFragmentForLogging(const net::IPAddressBytes& sender_ip);

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_METRICS_UTIL_H_
