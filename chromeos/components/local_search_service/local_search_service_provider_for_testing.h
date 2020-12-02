// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROVIDER_FOR_TESTING_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROVIDER_FOR_TESTING_H_

#include "chromeos/components/local_search_service/local_search_service.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_provider.h"

namespace chromeos {
namespace local_search_service {

// An implementation that runs LocalSearchService in-process for testing
// purpose.
class LocalSearchServiceProviderForTesting : public LocalSearchServiceProvider {
 public:
  LocalSearchServiceProviderForTesting();
  ~LocalSearchServiceProviderForTesting() override;

  // LocalSearchServiceProvider:
  void BindLocalSearchService(
      mojo::PendingReceiver<mojom::LocalSearchService> receiver) override;

 private:
  std::unique_ptr<LocalSearchService> service_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROVIDER_FOR_TESTING_H_
