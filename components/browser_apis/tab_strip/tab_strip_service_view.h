// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TAB_STRIP_SERVICE_VIEW_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TAB_STRIP_SERVICE_VIEW_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/browser_apis/tab_strip/observation/tab_strip_api_batched_observer.h"
#include "components/browser_apis/tab_strip/observation/tab_strip_api_observer.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs_api {

// Maintains a synchronized, read-only materialized view of a `TabStripService`
// by seeding from an initial snapshot and applying incremental
// `TabStripApiObserver` events. Provides O(1) point lookups and pre-order tab
// iteration without per-query snapshot serialization.
class TabStripServiceView : public observation::TabStripApiObserver {
 public:
  explicit TabStripServiceView(TabStripService& service);
  TabStripServiceView(const TabStripServiceView&) = delete;
  TabStripServiceView& operator=(const TabStripServiceView&) = delete;
  ~TabStripServiceView() override;

  // Returns the root container of the materialized tab strip tree, or nullptr
  // if the initial snapshot failed.
  const mojom::Container* root() const { return root_.get(); }

  // Returns the currently active tab, or nullptr if none is active.
  const Tab* GetActiveTab() const;

  // Returns the tab for `id` (or `handle`), or nullptr if not found.
  const Tab* GetTab(const NodeId& id) const;
  const Tab* GetTab(tabs::SupportsTabHandles::Handle handle) const;

  // Returns all tabs in pre-order tab strip traversal order.
  base::span<const raw_ptr<const Tab>> GetTabs() const { return ordered_tabs_; }

  // Returns the `mojom::Data` union for any node (tab, group, split, etc.), or
  // nullptr if not found.
  const mojom::Data* GetNodeData(const NodeId& id) const;

  // Returns the immediate parent collection's NodeId for `id`, or std::nullopt
  // if `id` is the root or not found.
  std::optional<NodeId> GetParentId(const NodeId& id) const;

  // observation::TabStripApiObserver:
  void OnTabsCreated(
      const mojom::OnTabsCreatedEventPtr& tabs_created_event) override;
  void OnNodesClosed(
      const mojom::OnNodesClosedEventPtr& nodes_closed_event) override;
  void OnNodeMoved(const mojom::OnNodeMovedEventPtr& node_moved_event) override;
  void OnDataChanged(
      const mojom::OnDataChangedEventPtr& data_changed_event) override;
  void OnCollectionCreated(const mojom::OnCollectionCreatedEventPtr&
                               collection_created_event) override;

 private:
  struct NodeEntry {
    raw_ptr<mojom::Container> container = nullptr;
    raw_ptr<mojom::Container> parent = nullptr;
  };

  Tab* FindMutableTab(const NodeId& id);
  mojom::Container* FindContainer(const NodeId& id);
  void SetActiveTab(const NodeId& id);
  mojom::Container* ResolveParentContainer(const Position& position);
  void IndexSubtree(mojom::Container* container, mojom::Container* parent);
  void UnindexSubtree(mojom::Container* container);
  void RebuildOrderedTabs();

  mojom::ContainerPtr root_;
  absl::flat_hash_map<NodeId, NodeEntry> nodes_by_id_;
  std::vector<raw_ptr<const Tab>> ordered_tabs_;
  std::optional<NodeId> active_tab_id_;

  base::ScopedObservation<TabStripService,
                          observation::TabStripApiBatchedObserver>
      observation_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TAB_STRIP_SERVICE_VIEW_H_
