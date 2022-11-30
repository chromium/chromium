// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_NET_NETWORK_H_
#define CHROME_UPDATER_MAC_NET_NETWORK_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "components/update_client/network.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

struct PolicyServiceProxyConfiguration;

class NetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  explicit NetworkFetcherFactory(absl::optional<PolicyServiceProxyConfiguration>
                                     policy_service_proxy_configuration);
  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactory() override;

 private:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_NET_NETWORK_H_
