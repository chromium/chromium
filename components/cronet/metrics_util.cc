// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/metrics_util.h"

#include "base/check.h"
#include "base/time/time.h"
#include "net/base/proxy_chain.h"

namespace cronet {

namespace metrics_util {

int64_t ConvertTime(const base::TimeTicks& ticks,
                    const base::TimeTicks& start_ticks,
                    const base::Time& start_time) {
  if (ticks.is_null() || start_ticks.is_null()) {
    return kNullTime;
  }
  DCHECK(!start_time.is_null());
  return (start_time + (ticks - start_ticks) - base::Time::UnixEpoch())
      .InMicroseconds();
}

std::string GetProxy(const net::ProxyChain& proxy_chain) {
  if (!proxy_chain.IsValid() || proxy_chain.is_direct()) {
    return net::HostPortPair().ToString();
  }
  CHECK(proxy_chain.is_single_proxy());
  return proxy_chain.First().host_port_pair().ToString();
}

bool IsProxied(const net::ProxyChain& proxy_chain) {
  return proxy_chain.IsValid() && !proxy_chain.is_direct();
}

}  // namespace metrics_util

}  // namespace cronet
