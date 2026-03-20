// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_MODEL_ADAPTER_H_

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/tab_states.h"

namespace tabs_api {

// A boring adapter impl that just gives up on everything. Maybe even itself.
class AndroidTabStripModelAdapter : public TabStripModelAdapter {
 public:
  explicit AndroidTabStripModelAdapter(TabModel* model);
  AndroidTabStripModelAdapter(const AndroidTabStripModelAdapter&&) = delete;
  AndroidTabStripModelAdapter operator=(const AndroidTabStripModelAdapter&) =
      delete;
  ~AndroidTabStripModelAdapter() override;

  std::vector<tabs::TabHandle> GetTabs() const override;
  types::TabStates GetTabStates(tabs::TabHandle) const override;
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
  tabs::TabCollectionHandle GetCollectionHandleForSplitTabId(
      split_tabs::SplitTabId split_id) const override;
  tabs_api::Position GetPositionForAbsoluteIndex(
      int absolute_index) const override;
  tabs_api::Path GetPathForCollection(
      tabs::TabCollectionHandle collection_handle) const override;
  InsertionParams CalculateInsertionParams(
      const std::optional<tabs_api::Position>& pos) const override;
  void ReplaceTabInSplit(tabs::TabHandle tab_to_replace,
                         int tab_to_insert_index) override;
  const tabs::TabCollection* GetRoot() const override;
  std::string GetWindowId() const override;

 private:
  raw_ref<TabModel> model_;
  std::unique_ptr<tabs::TabCollection> fake_root_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_MODEL_ADAPTER_H_
