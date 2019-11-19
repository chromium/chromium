// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_FUCHSIA_H_
#define CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_FUCHSIA_H_

#include "base/macros.h"
#include "net/base/network_change_notifier_factory.h"

namespace chromecast {

class NetworkChangeNotifierFactoryFuchsia
    : public net::NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactoryFuchsia();
  ~NetworkChangeNotifierFactoryFuchsia() override;

  // net::NetworkChangeNotifierFactory implementation:
  std::unique_ptr<net::NetworkChangeNotifier> CreateInstance() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierFactoryFuchsia);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_FUCHSIA_H_
