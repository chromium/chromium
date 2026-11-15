// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/event_logger.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/net/network_fetcher_libcurl.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"

namespace updater {

class NetworkFetcherFactory::Impl {};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>,
    scoped_refptr<UpdaterEventLogger>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<LibcurlNetworkFetcher> fetcher =
      LibcurlNetworkFetcher::Create();
  if (!fetcher) {
    return nullptr;
  }
  return std::make_unique<LoggingNetworkFetcher>(std::move(fetcher));
}

}  // namespace updater
