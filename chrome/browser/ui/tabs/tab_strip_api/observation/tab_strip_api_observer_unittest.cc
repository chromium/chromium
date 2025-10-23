// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_observer.h"

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::observation {
namespace {

class TrivialTabStripApiObserver : public TabStripApiObserver {
 public:
  TrivialTabStripApiObserver() = default;
  ~TrivialTabStripApiObserver() override = default;

  void OnTabsCreated(
      const mojom::OnTabsCreatedEventPtr& tabs_created_event) override {
    on_tabs_created_invoked = true;
  }

  void OnTabsClosed(
      const mojom::OnTabsClosedEventPtr& tabs_closed_event) override {
    on_tabs_closed_invoked = true;
  }

  void OnNodeMoved(
      const mojom::OnNodeMovedEventPtr& node_moved_event) override {
    on_node_moved_invoked = true;
  }

  void OnDataChanged(
      const mojom::OnDataChangedEventPtr& data_changed_event) override {
    on_data_changed_invoked = true;
  }

  void OnCollectionCreated(const mojom::OnCollectionCreatedEventPtr&
                               collection_created_event) override {
    on_collection_created_invoked = true;
  }

  bool on_tabs_created_invoked = false;
  bool on_tabs_closed_invoked = false;
  bool on_node_moved_invoked = false;
  bool on_data_changed_invoked = false;
  bool on_collection_created_invoked = false;
};

TEST(TabStripApiObserver, Dispatch) {
  std::vector<mojom::TabsEventPtr> buffer;

  {
    buffer.clear();
    TrivialTabStripApiObserver create_test;
    buffer.push_back(mojom::TabsEvent::NewTabsCreatedEvent(
        mojom::OnTabsCreatedEvent::New()));
    create_test.OnTabEvents(buffer);
    ASSERT_TRUE(create_test.on_tabs_created_invoked);
  }

  {
    buffer.clear();
    TrivialTabStripApiObserver closed_test;
    buffer.push_back(
        mojom::TabsEvent::NewTabsClosedEvent(mojom::OnTabsClosedEvent::New()));
    closed_test.OnTabEvents(buffer);
    ASSERT_TRUE(closed_test.on_tabs_closed_invoked);
  }

  {
    buffer.clear();
    TrivialTabStripApiObserver node_moved_test;
    buffer.push_back(
        mojom::TabsEvent::NewNodeMovedEvent(mojom::OnNodeMovedEvent::New()));
    node_moved_test.OnTabEvents(buffer);
    ASSERT_TRUE(node_moved_test.on_node_moved_invoked);
  }

  {
    buffer.clear();
    TrivialTabStripApiObserver data_changed_test;
    buffer.push_back(mojom::TabsEvent::NewDataChangedEvent(
        mojom::OnDataChangedEvent::New()));
    data_changed_test.OnTabEvents(buffer);
    ASSERT_TRUE(data_changed_test.on_data_changed_invoked);
  }

  {
    buffer.clear();
    TrivialTabStripApiObserver collection_created_test;
    buffer.push_back(mojom::TabsEvent::NewCollectionCreatedEvent(
        mojom::OnCollectionCreatedEvent::New()));
    collection_created_test.OnTabEvents(buffer);
    ASSERT_TRUE(collection_created_test.on_collection_created_invoked);
  }
}

}  // namespace
}  // namespace tabs_api::observation
