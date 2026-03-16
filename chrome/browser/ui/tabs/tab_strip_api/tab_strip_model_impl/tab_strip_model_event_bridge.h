// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_EVENT_BRIDGE_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_observer.h"

namespace tabs_api::tab_strip_model {

// TODO(ffred): I think this design is probably more complicated than it needs
// to be. Maybe we could simplify the bridge between TSM and the service.
//
// Binds an EventObserver to the TabStripModel. There are some annoyances with
// the event typing which prevents reuse. Each instance of a bridge has two
// ends: an EventObserver and a TSM observer.
class BridgeInstance : public TabStripModelObserver,
                       public tabs::TabCollectionObserver {
 public:
  BridgeInstance(TabStripModelAdapterImpl& tab_strip_model_adapter,
                 events::EventObserver* observer);
  BridgeInstance(const BridgeInstance&&) = delete;
  BridgeInstance operator=(const BridgeInstance&) = delete;
  ~BridgeInstance() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      int index,
                      TabChangeType change_type) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // tabs::TabCollectionObserver:
  void OnChildrenAdded(const tabs::TabCollection::Position& position,
                       const tabs::TabCollectionNodes& handles,
                       bool insert_from_detached) override;

  void OnChildrenRemoved(const tabs::TabCollection::Position& position,
                         const tabs::TabCollectionNodes& handles) override;

  void OnChildMoved(const tabs::TabCollection::Position& to_position,
                    const NodeData& node_data) override;

 private:
  void ForwardToObserver(events::Event event);
  void ForwardToObserver(std::vector<events::Event> events);

  raw_ref<TabStripModelAdapterImpl> tab_strip_model_adapter_;
  raw_ptr<events::EventObserver> observer_;
};

class TabStripModelEventBridge : public EventBridge {
 public:
  explicit TabStripModelEventBridge(
      TabStripModelAdapterImpl& tab_strip_model_adapter);
  TabStripModelEventBridge(const TabStripModelEventBridge&&) = delete;
  TabStripModelEventBridge operator=(const TabStripModelEventBridge&) = delete;
  ~TabStripModelEventBridge() override;

  // EventBridge:
  void AddObserver(events::EventObserver* observer) override;
  void RemoveObserver(events::EventObserver* observer) override;

 private:
  raw_ref<TabStripModelAdapterImpl> tab_strip_model_adapter_;
  std::map<events::EventObserver*, std::unique_ptr<BridgeInstance>>
      observer_to_bridge_;
};

}  // namespace tabs_api::tab_strip_model

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_EVENT_BRIDGE_H_
