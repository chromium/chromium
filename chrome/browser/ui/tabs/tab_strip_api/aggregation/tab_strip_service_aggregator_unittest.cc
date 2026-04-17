// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_aggregator.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_service_tracker.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {

namespace {

// A helper to manage a real TabStripServiceImpl with its dependencies.
class ManagedTabStripService {
 public:
  ManagedTabStripService() {
    tab_strip_ = std::make_unique<testing::ToyTabStrip>();
    auto injector = std::make_unique<testing::Injector>(*tab_strip_);
    event_bridge_ = &injector->event_bridge();
    service_ =
        std::make_unique<TabStripServiceImpl>(std::move(injector), nullptr);
  }

  ~ManagedTabStripService() {
    service_.reset();
    event_bridge_ = nullptr;
  }

  TabStripService* service() const { return service_.get(); }

  void TriggerEvents(const std::vector<events::Event>& events) {
    event_bridge_->NotifyEvents(events);
  }

 private:
  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  raw_ptr<testing::ToyTabStripEventBridge, DisableDanglingPtrDetection>
      event_bridge_;
  std::unique_ptr<TabStripServiceImpl> service_;
};

class TestBatchedObserver {
 public:
  TestBatchedObserver() = default;
  ~TestBatchedObserver() = default;

  void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) {
    for (const auto& event : events) {
      received_events_.push_back(event.Clone());
    }
  }

  const std::vector<mojom::TabsEventPtr>& received_events() const {
    return received_events_;
  }

 private:
  std::vector<mojom::TabsEventPtr> received_events_;
};

}  // namespace

class TabStripServiceAggregatorTest : public ::testing::Test {
 public:
  TabStripServiceAggregatorTest() = default;
  ~TabStripServiceAggregatorTest() override {
    tracker_ptr_ = nullptr;
    aggregator_.reset();
  }

  void SetUp() override {
    auto tracker = std::make_unique<testing::ToyTabStripServiceTracker>();
    tracker_ptr_ = tracker.get();
    aggregator_ = std::make_unique<TabStripServiceAggregator>(
        std::move(tracker),
        base::BindRepeating(&TestBatchedObserver::OnTabEvents,
                            base::Unretained(&delegate_)));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestBatchedObserver delegate_;
  raw_ptr<testing::ToyTabStripServiceTracker> tracker_ptr_;
  std::unique_ptr<TabStripServiceAggregator> aggregator_;
};

TEST_F(TabStripServiceAggregatorTest, ObservesServices) {
  ManagedTabStripService managed1;
  ManagedTabStripService managed2;

  auto tracker = std::make_unique<testing::ToyTabStripServiceTracker>();
  tracker->AddService(managed1.service());
  tracker->AddService(managed2.service());

  TestBatchedObserver delegate;
  TabStripServiceAggregator aggregator(
      std::move(tracker), base::BindRepeating(&TestBatchedObserver::OnTabEvents,
                                              base::Unretained(&delegate)));

  std::vector<events::Event> events1;
  events1.push_back(mojom::OnTabsCreatedEvent::New());
  events1.push_back(mojom::OnNodeMovedEvent::New());
  managed1.TriggerEvents(events1);

  std::vector<events::Event> events2;
  events2.push_back(mojom::OnNodesClosedEvent::New());
  managed2.TriggerEvents(events2);

  ASSERT_EQ(delegate.received_events().size(), 3u);
  ASSERT_TRUE(delegate.received_events()[0]->is_tabs_created_event());
  ASSERT_TRUE(delegate.received_events()[1]->is_node_moved_event());
  ASSERT_TRUE(delegate.received_events()[2]->is_nodes_closed_event());
}

TEST_F(TabStripServiceAggregatorTest, ObservesAddedServices) {
  ManagedTabStripService managed;
  tracker_ptr_->AddService(managed.service());

  std::vector<events::Event> events;
  events.push_back(mojom::OnNodeMovedEvent::New());
  managed.TriggerEvents(events);

  ASSERT_EQ(delegate_.received_events().size(), 1u);
  ASSERT_TRUE(delegate_.received_events()[0]->is_node_moved_event());

  tracker_ptr_->RemoveService(managed.service());
}

TEST_F(TabStripServiceAggregatorTest, StopsObservingRemovedServices) {
  ManagedTabStripService managed;
  tracker_ptr_->AddService(managed.service());
  tracker_ptr_->RemoveService(managed.service());

  std::vector<events::Event> events;
  events.push_back(mojom::OnTabsCreatedEvent::New());
  managed.TriggerEvents(events);

  // No events should be received since the service was removed.
  ASSERT_EQ(delegate_.received_events().size(), 0u);
}

}  // namespace tabs_api
