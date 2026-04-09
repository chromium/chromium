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

class TabStripModelEventBridge : public EventBridge,
                                 public TabStripModelObserver,
                                 public tabs::TabCollectionObserver {
 public:
  explicit TabStripModelEventBridge(
      TabStripModelAdapterImpl& tab_strip_model_adapter);
  TabStripModelEventBridge(const TabStripModelEventBridge&&) = delete;
  TabStripModelEventBridge operator=(const TabStripModelEventBridge&) = delete;
  ~TabStripModelEventBridge() override;

  // EventBridge:
  void AddObserver(events::EventObserver* observer) override;
  void RemoveObserver(events::EventObserver* observer) override;

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
  void Notify(const events::Event& event) const;
  void Notify(const std::vector<events::Event>& events) const;

  base::ObserverList<events::EventObserver> observers_;
  raw_ref<TabStripModelAdapterImpl> tab_strip_model_adapter_;
};

}  // namespace tabs_api::tab_strip_model

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_STRIP_MODEL_EVENT_BRIDGE_H_
