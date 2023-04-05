// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_
#define CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_

#include "base/compiler_specific.h"
#include "net/base/network_change_notifier_factory.h"

namespace chromecast {

class NetworkChangeNotifierFactoryCast
    : public net::NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactoryCast() = default;

  NetworkChangeNotifierFactoryCast(const NetworkChangeNotifierFactoryCast&) =
      delete;
  NetworkChangeNotifierFactoryCast& operator=(
      const NetworkChangeNotifierFactoryCast&) = delete;

  ~NetworkChangeNotifierFactoryCast() override;

  // net::NetworkChangeNotifierFactory implementation:
  std::unique_ptr<net::NetworkChangeNotifier> CreateInstanceWithInitialTypes(
      net::NetworkChangeNotifier::ConnectionType /*initial_type*/,
      net::NetworkChangeNotifier::ConnectionSubtype /*initial_subtype*/)
      override;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_
