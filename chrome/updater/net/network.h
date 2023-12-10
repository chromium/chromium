// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_NETWORK_H_
#define CHROME_UPDATER_NET_NETWORK_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/update_client/network.h"

namespace updater {

struct PolicyServiceProxyConfiguration;

// Creates instances of `NetworkFetcher`. Because of idiosyncrasies of how
// the Windows implementation works, the instance of the factory class must
// outlive the lives of the network fetchers it creates.
class NetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  explicit NetworkFetcherFactory(std::optional<PolicyServiceProxyConfiguration>
                                     policy_service_proxy_configuration);
  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactory() override;

 private:
  class Impl;

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<Impl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_NETWORK_H_
