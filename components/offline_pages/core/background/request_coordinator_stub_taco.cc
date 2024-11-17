// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/background/scheduler.h"
#include "components/offline_pages/core/background/scheduler_stub.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "services/network/test/test_network_quality_tracker.h"

namespace offline_pages {

namespace {

class ActiveTabInfo : public RequestCoordinator::ActiveTabInfo {
 public:
  ~ActiveTabInfo() override = default;
  bool DoesActiveTabMatch(const GURL&) override { return false; }
};

}  // namespace

RequestCoordinatorStubTaco::RequestCoordinatorStubTaco() {
  policy_ = std::make_unique<OfflinerPolicy>();
  queue_ =
      std::make_unique<RequestQueue>(std::make_unique<TestRequestQueueStore>());
  offliner_ = std::make_unique<OfflinerStub>();
  scheduler_ = std::make_unique<SchedulerStub>();
  network_quality_tracker_ =
      std::make_unique<network::TestNetworkQualityTracker>();
  active_tab_info_ = std::make_unique<ActiveTabInfo>();
}

RequestCoordinatorStubTaco::~RequestCoordinatorStubTaco() {
}

void RequestCoordinatorStubTaco::SetOfflinerPolicy(
    std::unique_ptr<OfflinerPolicy> policy) {
  CHECK(!request_coordinator_);
  policy_ = std::move(policy);
}

void RequestCoordinatorStubTaco::SetRequestQueueStore(
    std::unique_ptr<RequestQueueStore> store) {
  CHECK(!request_coordinator_ && !queue_overridden_);
  store_overridden_ = true;
  queue_ = std::make_unique<RequestQueue>(std::move(store));
}

void RequestCoordinatorStubTaco::SetRequestQueue(
    std::unique_ptr<RequestQueue> queue) {
  CHECK(!request_coordinator_ && !store_overridden_);
  queue_overridden_ = true;
  queue_ = std::move(queue);
}

void RequestCoordinatorStubTaco::SetOffliner(
    std::unique_ptr<Offliner> offliner) {
  CHECK(!request_coordinator_);
  offliner_ = std::move(offliner);
}

void RequestCoordinatorStubTaco::SetScheduler(
    std::unique_ptr<Scheduler> scheduler) {
  CHECK(!request_coordinator_);
  scheduler_ = std::move(scheduler);
}

void RequestCoordinatorStubTaco::SetNetworkQualityProvider(
    std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker) {
  CHECK(!request_coordinator_);
  network_quality_tracker_ = std::move(network_quality_tracker);
}

void RequestCoordinatorStubTaco::SetRequestCoordinatorDelegate(
    std::unique_ptr<RequestCoordinator::ActiveTabInfo> active_tab_info) {
  active_tab_info_ = std::move(active_tab_info);
}

void RequestCoordinatorStubTaco::CreateRequestCoordinator() {
  CHECK(!request_coordinator_)
      << "CreateRequestCoordinator can be called only once";
  owned_request_coordinator_ = std::make_unique<RequestCoordinator>(
      std::move(policy_), std::move(offliner_), std::move(queue_),
      std::move(scheduler_), network_quality_tracker_.get(),
      std::move(active_tab_info_));
  request_coordinator_ = owned_request_coordinator_.get();
}

RequestCoordinator* RequestCoordinatorStubTaco::request_coordinator() {
  CHECK(request_coordinator_);
  return request_coordinator_;
}

base::RepeatingCallback<std::unique_ptr<KeyedService>(content::BrowserContext*)>
RequestCoordinatorStubTaco::FactoryFunction() {
  return base::BindRepeating(
      &RequestCoordinatorStubTaco::InternalFactoryFunction, GetWeakPtr());
}

// static
std::unique_ptr<KeyedService>
RequestCoordinatorStubTaco::InternalFactoryFunction(
    base::WeakPtr<RequestCoordinatorStubTaco> taco,
    content::BrowserContext* context) {
  if (!taco)
    return nullptr;
  // Call CreateRequestCoordinator if it hasn't already been called.
  if (!taco->request_coordinator_) {
    taco->CreateRequestCoordinator();
  }
  // This function can only be used once.
  CHECK(taco->owned_request_coordinator_);
  return std::move(taco->owned_request_coordinator_);
}

}  // namespace offline_pages
