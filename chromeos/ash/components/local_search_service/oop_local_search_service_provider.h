// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_provider.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom-forward.h"
namespace chromeos {
namespace local_search_service {

// An implementation that runs LocalSearchService in the LSS service
// process.
class OopLocalSearchServiceProvider : public LocalSearchServiceProvider {
 public:
  OopLocalSearchServiceProvider();
  ~OopLocalSearchServiceProvider() override;

  // LocalSearchServiceProvider:
  void BindLocalSearchService(
      mojo::PendingReceiver<mojom::LocalSearchService> receiver) override;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
