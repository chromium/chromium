// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/net/network.h"

#include <memory>

#include "base/notreached.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

NetworkFetcherFactory::NetworkFetcherFactory(
    absl::optional<PolicyServiceProxyConfiguration>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
