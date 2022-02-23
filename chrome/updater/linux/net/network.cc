// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/net/network.h"

#include <memory>

#include "base/notreached.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"

namespace updater {

NetworkFetcherFactory::NetworkFetcherFactory(
    scoped_refptr<PolicyService> /*policy_service*/) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
