// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"

#include <memory>
#include <string>

#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "services/network/test/test_shared_url_loader_factory.h"

namespace offline_pages {
namespace {
version_info::Channel kChannel = version_info::Channel::UNKNOWN;
const char kUserAgent[] = "Chrome/57.0.2987.133";
}  // namespace

TestPrefetchNetworkRequestFactory::TestPrefetchNetworkRequestFactory(
    PrefService* prefs)
    : TestPrefetchNetworkRequestFactory(new network::TestSharedURLLoaderFactory,
                                        prefs) {}

TestPrefetchNetworkRequestFactory::TestPrefetchNetworkRequestFactory(
    scoped_refptr<network::SharedURLLoaderFactory> loader,
    PrefService* prefs)
    : PrefetchNetworkRequestFactoryImpl(loader, kChannel, kUserAgent, prefs) {
  url_loader_factory = loader;
}

TestPrefetchNetworkRequestFactory::~TestPrefetchNetworkRequestFactory() =
    default;

}  // namespace offline_pages
