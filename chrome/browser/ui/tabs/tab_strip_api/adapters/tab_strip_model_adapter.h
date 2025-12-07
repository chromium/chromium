// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_

#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api {

namespace converters {
struct TabStates;
}  // namespace converters

struct InsertionParams {
  std::optional<int> index;
  std::optional<tab_groups::TabGroupId> group_id;
  bool pinned = false;
};

// Tab strip has a large API service that is difficult to implement under test.
// We only need a subset of the API, so an adapter is used to proxy those
// methods. This makes it easier to swap in a fake for test.
class TabStripModelAdapter {
 public:
  virtual ~TabStripModelAdapter() {}

  virtual void AddModelObserver(TabStripModelObserver* observer) = 0;
  virtual void RemoveModelObserver(TabStripModelObserver* observer) = 0;
  virtual void AddCollectionObserver(
      tabs::TabCollectionObserver* collection_observer) = 0;
  virtual void RemoveCollectionObserver(
      tabs::TabCollectionObserver* collection_observer) = 0;
  virtual std::vector<tabs::TabHandle> GetTabs() const = 0;
  virtual TabRendererData GetTabRendererData(int index) const = 0;
  virtual converters::TabStates GetTabStates(tabs::TabHandle) const = 0;
  virtual const ui::ColorProvider& GetColorProvider() const = 0;
  virtual void CloseTab(size_t tab_index) = 0;
  virtual std::optional<int> GetIndexForHandle(
      tabs::TabHandle tab_handle) const = 0;
  virtual void ActivateTab(size_t index) = 0;
  virtual void MoveTab(tabs::TabHandle handle, const Position& position) = 0;
  virtual void MoveCollection(const NodeId& id, const Position& position) = 0;
  virtual mojom::ContainerPtr GetTabStripTopology(
      tabs::TabCollection::Handle root) const = 0;
  virtual std::optional<const tab_groups::TabGroupId> FindGroupIdFor(
      const tabs::TabCollection::Handle& collection_handle) const = 0;
  virtual void UpdateTabGroupVisuals(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) = 0;
  virtual void SetTabSelection(
      const std::vector<tabs::TabHandle>& handles_to_select,
      tabs::TabHandle to_activate) = 0;
  virtual std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const = 0;
  virtual tabs::TabCollectionHandle GetCollectionHandleForTabGroupId(
      tab_groups::TabGroupId group_id) const = 0;
  virtual tabs_api::Position GetPositionForAbsoluteIndex(
      int absolute_index) const = 0;
  virtual InsertionParams CalculateInsertionParams(
      const std::optional<tabs_api::Position>& pos) const = 0;
  virtual const tabs::TabCollection* GetRoot() const = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_
