// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs_api {

// A simple forwarder proxy for the tab strip model. Avoid adding logic to this
// class. It should *only* forward requests to the tab strip model.
class TabStripModelAdapterImpl : public TabStripModelAdapter {
 public:
  explicit TabStripModelAdapterImpl(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {}
  TabStripModelAdapterImpl(const TabStripModelAdapterImpl&&) = delete;
  TabStripModelAdapterImpl operator=(const TabStripModelAdapterImpl&) = delete;
  ~TabStripModelAdapterImpl() override {}

  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;
  std::vector<tabs::TabHandle> GetTabs() const override;
  TabRendererData GetTabRendererData(int index) const override;
  void CloseTab(size_t tab_index) override;
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) override;
  void ActivateTab(size_t index) override;
  void MoveTab(tabs::TabHandle handle, const Position& position) override;
  void MoveCollection(const NodeId& id, const Position& position) override;
  mojom::TabCollectionContainerPtr GetTabStripTopology() override;
  std::optional<const tab_groups::TabGroupId> FindGroupIdFor(
      const tabs::TabCollection::Handle& collection_handle) override;
  void UpdateTabGroupVisuals(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  // TabStripModelAdapterImpl uses passkeys to access experimental API methods
  // in TabStripModel or TabCollections.
  // PassKeyForTesting provides a passkey for testing purposes. Note that by
  // using PassKeyForTesting, it deeply couples the test class to this class
  // which breaks the loose coupling benefit of passkeys.
  static base::PassKey<TabStripModelAdapterImpl> PassKeyForTesting() {
    return base::PassKey<TabStripModelAdapterImpl>();
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_
