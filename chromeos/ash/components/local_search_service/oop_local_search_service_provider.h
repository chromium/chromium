// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_provider.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom-forward.h"

namespace ash::local_search_service {

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

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_OOP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
