// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/hints_fetcher_factory.h"

#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

HintsFetcherFactory::HintsFetcherFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& optimization_guide_service_url,
    PrefService* pref_service,
    network::NetworkConnectionTracker* network_connection_tracker)
    : url_loader_factory_(url_loader_factory),
      optimization_guide_service_url_(optimization_guide_service_url),
      pref_service_(pref_service),
      network_connection_tracker_(network_connection_tracker) {}

HintsFetcherFactory::~HintsFetcherFactory() = default;

std::unique_ptr<HintsFetcher> HintsFetcherFactory::BuildInstance() {
  return std::make_unique<HintsFetcher>(
      url_loader_factory_, optimization_guide_service_url_, pref_service_,
      network_connection_tracker_);
}

}  // namespace optimization_guide
