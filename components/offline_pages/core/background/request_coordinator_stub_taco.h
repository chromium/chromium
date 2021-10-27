// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_STUB_TACO_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_STUB_TACO_H_

#include <memory>

#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/scheduler.h"

namespace content {
class BrowserContext;
}
namespace network {
class NetworkQualityTracker;
}

namespace offline_pages {

// The taco class acts as a wrapper around the request coordinator making
// it easy to create for tests, using stub versions of the dependencies.
// This class is like a taco shell, and the filling is the request coordinator.
// The default dependencies may be replaced by the test author to provide
// custom versions that have test-specific hooks.
class RequestCoordinatorStubTaco {
 public:
  RequestCoordinatorStubTaco();
  ~RequestCoordinatorStubTaco();

  // These methods must be called before CreateRequestCoordinator() is invoked.
  // If called after they will CHECK().
  void SetOfflinerPolicy(std::unique_ptr<OfflinerPolicy> policy);
  // Note: it makes sense to only override a default store or the queue, but not
  // the both since setting the store will auto-create a default queue with it.
  // Conflicting usage will CHECK().
  void SetRequestQueueStore(std::unique_ptr<RequestQueueStore> store);
  void SetRequestQueue(std::unique_ptr<RequestQueue> queue);
  void SetOffliner(std::unique_ptr<Offliner> offliner);
  void SetScheduler(std::unique_ptr<Scheduler> scheduler);
  void SetNetworkQualityProvider(
      std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker);
  void SetRequestCoordinatorDelegate(
      std::unique_ptr<RequestCoordinator::ActiveTabInfo> delegate);

  // Creates and caches an instance of RequestCoordinator, using default or
  // overridden stub dependencies.
  void CreateRequestCoordinator();

  // Once CreateRequestCoordinator() is called, this accessor method start
  // returning the RequestCoordinator.
  // If called before CreateRequestCoordinator(), it will CHECK().

  RequestCoordinator* request_coordinator();

  // A factory function that can be used with
  // RequestCoordinatorFactory::SetTestingFactoryAndUse.
  base::RepeatingCallback<
      std::unique_ptr<KeyedService>(content::BrowserContext*)>
  FactoryFunction();

 private:
  base::WeakPtr<RequestCoordinatorStubTaco> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static std::unique_ptr<KeyedService> InternalFactoryFunction(
      base::WeakPtr<RequestCoordinatorStubTaco> taco,
      content::BrowserContext*);

  bool store_overridden_ = false;
  bool queue_overridden_ = false;

  std::unique_ptr<OfflinerPolicy> policy_;
  std::unique_ptr<RequestQueue> queue_;
  std::unique_ptr<Offliner> offliner_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker_;
  std::unique_ptr<RequestCoordinator::ActiveTabInfo> active_tab_info_;

  // This is null if the request coordinator was given to the
  // RequestCoordinatorFactory through the factory function.
  std::unique_ptr<RequestCoordinator> owned_request_coordinator_;
  RequestCoordinator* request_coordinator_ = nullptr;

  base::WeakPtrFactory<RequestCoordinatorStubTaco> weak_ptr_factory_{this};
};
}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_STUB_TACO_H_
