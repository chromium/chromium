// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_NETWORK_REQUEST_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_NETWORK_REQUEST_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/version_info/channel.h"

class PrefService;

namespace offline_pages {

// Test factory that uses a TestURLRequestContextGetter.
// manipulation.
class TestPrefetchNetworkRequestFactory
    : public PrefetchNetworkRequestFactoryImpl {
 public:
  TestPrefetchNetworkRequestFactory(PrefService* prefs);
  explicit TestPrefetchNetworkRequestFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* prefs);
  ~TestPrefetchNetworkRequestFactory() override;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_NETWORK_REQUEST_FACTORY_H_
