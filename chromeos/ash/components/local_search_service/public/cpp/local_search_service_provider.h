// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROVIDER_H_

#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::local_search_service {

// LocalSearchServiceProvider creates an instance of LocalSearchService
// and runs in LSS service process or in process (depending on the
// implementation).
class LocalSearchServiceProvider {
 public:
  virtual ~LocalSearchServiceProvider() {}

  // Sets the global LocalSearchServiceProvider instance.
  // Specifically, there will be a global |g_provider| in this class' .cpp file
  // and in the anonymous namespace. The Set function will set |g_provider| to
  // the input |provider|.
  // This function must be called before the service is requested.
  static void Set(LocalSearchServiceProvider* provider);
  static LocalSearchServiceProvider* Get();

  // Binds |receiver| to an instance of the LocalSearchService.
  // Each call to this method will request a new instance of the service.
  virtual void BindLocalSearchService(
      mojo::PendingReceiver<mojom::LocalSearchService> receiver) = 0;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROVIDER_H_
