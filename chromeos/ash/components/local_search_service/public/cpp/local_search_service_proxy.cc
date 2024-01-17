// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/local_search_service/local_search_service_provider_for_testing.h"
#include "chromeos/ash/components/local_search_service/oop_local_search_service_provider.h"
#include "components/prefs/pref_service.h"

namespace ash::local_search_service {

namespace {

void OnBindIndexDone(const std::optional<std::string>& error) {
  base::UmaHistogramBoolean("LocalSearchService.BindIndexHasError",
                            error.has_value());
  if (error)
    LOG(ERROR) << "BindIndex error: " << error.value();
}

}  // namespace

LocalSearchServiceProxy::LocalSearchServiceProxy(bool for_testing) {
  if (!for_testing) {
    // Create an instance of OopLocalSearchServiceProvider.
    // This will set |g_provider|.
    local_search_service_provider_ =
        std::make_unique<OopLocalSearchServiceProvider>();
  } else {
    local_search_service_provider_ =
        std::make_unique<LocalSearchServiceProviderForTesting>();
  }
}

LocalSearchServiceProxy::~LocalSearchServiceProxy() = default;

void LocalSearchServiceProxy::SetLocalState(PrefService* local_state) {
  DCHECK(local_state);
  if (!reporter_) {
    reporter_ = std::make_unique<SearchMetricsReporter>(local_state);
  }
}

void LocalSearchServiceProxy::GetIndex(
    IndexId index_id,
    Backend backend,
    mojo::PendingReceiver<mojom::Index> index_receiver) {
  auto* service = GetService();
  if (!service) {
    // In this case, client's mojom receiver will not be bound. Hence the
    // the client should always check it before using it.
    return;
  }

  service->BindIndex(
      index_id, backend, std::move(index_receiver),
      reporter_ ? reporter_->BindNewPipeAndPassRemote() : mojo::NullRemote(),
      base::BindOnce(&OnBindIndexDone));
}

mojom::LocalSearchService* LocalSearchServiceProxy::GetService() {
  if (!service_) {
    auto* provider = LocalSearchServiceProvider::Get();
    if (provider) {
      provider->BindLocalSearchService(service_.BindNewPipeAndPassReceiver());
    } else {
      LOG(FATAL) << "LocalSearchServiceProvider::Set() must be called "
                 << "before any instances of LocalSearchService can be used.";
    }
    service_.reset_on_disconnect();
  }

  return service_.get();
}

}  // namespace ash::local_search_service
