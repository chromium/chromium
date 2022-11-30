// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/local_search_service_provider_for_testing.h"

#include <memory>

namespace ash::local_search_service {

LocalSearchServiceProviderForTesting::LocalSearchServiceProviderForTesting() {
  LocalSearchServiceProvider::Set(this);
}

LocalSearchServiceProviderForTesting::~LocalSearchServiceProviderForTesting() {
  LocalSearchServiceProvider::Set(nullptr);
}

void LocalSearchServiceProviderForTesting::BindLocalSearchService(
    mojo::PendingReceiver<mojom::LocalSearchService> receiver) {
  service_ = std::make_unique<LocalSearchService>(std::move(receiver));
}

}  // namespace ash::local_search_service
