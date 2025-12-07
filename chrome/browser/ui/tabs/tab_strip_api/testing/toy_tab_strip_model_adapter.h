// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "ui/color/color_provider.h"

namespace tabs_api::testing {

class ToyTabStripModelAdapter : public TabStripModelAdapter {
 public:
  explicit ToyTabStripModelAdapter(ToyTabStrip* tab_strip);
  ToyTabStripModelAdapter(const ToyTabStripModelAdapter&) = delete;
  ToyTabStripModelAdapter operator=(const ToyTabStripModelAdapter&&) = delete;
  ~ToyTabStripModelAdapter() override = default;

  void AddModelObserver(TabStripModelObserver* observer) override;
  void RemoveModelObserver(TabStripModelObserver* observer) override;
  void AddCollectionObserver(
      tabs::TabCollectionObserver* collection_observer) override;
  void RemoveCollectionObserver(
      tabs::TabCollectionObserver* collection_observer) override;
  std::vector<tabs::TabHandle> GetTabs() const override;
  TabRendererData GetTabRendererData(int index) const override;
  tabs_api::converters::TabStates GetTabStates(
      tabs::TabHandle handle) const override;
  const ui::ColorProvider& GetColorProvider() const override;
  void CloseTab(size_t tab_index) override;
  std::optional<int> GetIndexForHandle(
      tabs::TabHandle tab_handle) const override;
  void ActivateTab(size_t index) override;
  void MoveTab(tabs::TabHandle handle, const Position& position) override;
  void MoveCollection(const NodeId& id, const Position& position) override;
  mojom::ContainerPtr GetTabStripTopology(
      tabs::TabCollection::Handle root) const override;
  std::optional<const tab_groups::TabGroupId> FindGroupIdFor(
      const tabs::TabCollection::Handle& collection_handle) const override;
  void UpdateTabGroupVisuals(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  void SetTabSelection(const std::vector<tabs::TabHandle>& handles_to_select,
                       tabs::TabHandle to_activate) override;
  std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  tabs::TabCollectionHandle GetCollectionHandleForTabGroupId(
      tab_groups::TabGroupId group_id) const override;
  tabs_api::Position GetPositionForAbsoluteIndex(
      int absolute_index) const override;
  InsertionParams CalculateInsertionParams(
      const std::optional<tabs_api::Position>& pos) const override;
  const tabs::TabCollection* GetRoot() const override;

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
  ui::ColorProvider color_provider_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_
