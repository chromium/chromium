// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_NET_NETWORK_H_
#define CHROME_UPDATER_LINUX_NET_NETWORK_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"

namespace updater {

class PolicyService;

// Network fetcher factory for WinHTTP.
class NetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  explicit NetworkFetcherFactory(scoped_refptr<PolicyService> policy_service);
  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactory() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_NET_NETWORK_H_
