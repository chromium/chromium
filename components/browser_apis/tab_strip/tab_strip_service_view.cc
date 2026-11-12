// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/tab_strip_service_view.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "components/browser_apis/tab_strip/utilities/tab_strip_api_utilities.h"

namespace tabs_api {

namespace {

void AppendTabs(const mojom::Container& container,
                std::vector<raw_ptr<const Tab>>& out) {
  if (container.data && container.data->is_tab()) {
    out.push_back(&container.data->get_tab());
  }
  for (const auto& child : container.children) {
    if (child) {
      AppendTabs(*child, out);
    }
  }
}

mojom::Container* InsertChildAt(mojom::Container& parent,
                                size_t index,
                                mojom::ContainerPtr node) {
  mojom::Container* raw = node.get();
  const size_t clamped = std::min(index, parent.children.size());
  parent.children.insert(
      parent.children.begin() + static_cast<ptrdiff_t>(clamped),
      std::move(node));
  return raw;
}

}  // namespace

TabStripServiceView::TabStripServiceView(TabStripService& service) {
  auto result = service.GetTabsWithoutObservation();
  observation_.Observe(&service);
  if (result.has_value() && *result) {
    root_ = std::move(*result);
    IndexSubtree(root_.get(), /*parent=*/nullptr);
    RebuildOrderedTabs();
  }
}

TabStripServiceView::~TabStripServiceView() {
  observation_.Reset();
  ordered_tabs_.clear();
  nodes_by_id_.clear();
  root_.reset();
}

const Tab* TabStripServiceView::GetActiveTab() const {
  return active_tab_id_ ? GetTab(*active_tab_id_) : nullptr;
}

const Tab* TabStripServiceView::GetTab(const NodeId& id) const {
  const mojom::Data* data = GetNodeData(id);
  return (data && data->is_tab()) ? &data->get_tab() : nullptr;
}

const Tab* TabStripServiceView::GetTab(
    tabs::SupportsTabHandles::Handle handle) const {
  return GetTab(NodeId::FromTabHandle(handle));
}

const mojom::Data* TabStripServiceView::GetNodeData(const NodeId& id) const {
  auto it = nodes_by_id_.find(id);
  return (it != nodes_by_id_.end() && it->second.container)
             ? it->second.container->data.get()
             : nullptr;
}

std::optional<NodeId> TabStripServiceView::GetParentId(const NodeId& id) const {
  auto it = nodes_by_id_.find(id);
  if (it == nodes_by_id_.end() || !it->second.parent ||
      !it->second.parent->data) {
    return std::nullopt;
  }
  return utils::GetNodeId(*it->second.parent->data);
}

void TabStripServiceView::OnTabsCreated(
    const mojom::OnTabsCreatedEventPtr& tabs_created_event) {
  if (!tabs_created_event) {
    return;
  }
  ordered_tabs_.clear();
  for (const auto& created : tabs_created_event->tabs) {
    mojom::Container* parent =
        created ? ResolveParentContainer(created->position) : nullptr;
    if (!parent) {
      continue;
    }
    auto node = mojom::Container::New();
    node->data = mojom::Data::NewTab(created->tab);
    IndexSubtree(
        InsertChildAt(*parent, created->position.index(), std::move(node)),
        parent);
  }
  RebuildOrderedTabs();
}

void TabStripServiceView::OnNodesClosed(
    const mojom::OnNodesClosedEventPtr& nodes_closed_event) {
  if (!nodes_closed_event) {
    return;
  }
  ordered_tabs_.clear();
  for (const NodeId& id : nodes_closed_event->node_ids) {
    auto it = nodes_by_id_.find(id);
    if (it == nodes_by_id_.end()) {
      continue;
    }
    mojom::Container* target = it->second.container;
    mojom::Container* parent = it->second.parent;
    UnindexSubtree(target);
    if (parent) {
      std::erase_if(parent->children, [target](const mojom::ContainerPtr& c) {
        return c.get() == target;
      });
    } else if (root_.get() == target) {
      root_.reset();
    }
  }
  RebuildOrderedTabs();
}

void TabStripServiceView::OnNodeMoved(
    const mojom::OnNodeMovedEventPtr& node_moved_event) {
  if (!node_moved_event) {
    return;
  }
  auto it = nodes_by_id_.find(node_moved_event->id);
  if (it == nodes_by_id_.end() || !it->second.container || !it->second.parent) {
    return;
  }
  mojom::Container* target = it->second.container;
  mojom::Container* old_parent = it->second.parent;
  mojom::Container* new_parent =
      node_moved_event->to.path().components().empty()
          ? old_parent
          : ResolveParentContainer(node_moved_event->to);
  auto child_it = std::ranges::find_if(
      old_parent->children,
      [target](const mojom::ContainerPtr& c) { return c.get() == target; });
  if (!new_parent || child_it == old_parent->children.end()) {
    return;
  }
  ordered_tabs_.clear();
  mojom::ContainerPtr moved = std::move(*child_it);
  old_parent->children.erase(child_it);
  InsertChildAt(*new_parent, node_moved_event->to.index(), std::move(moved));
  it->second.parent = new_parent;
  RebuildOrderedTabs();
}

void TabStripServiceView::OnDataChanged(
    const mojom::OnDataChangedEventPtr& data_changed_event) {
  if (!data_changed_event) {
    return;
  }
  switch (data_changed_event->which()) {
    case mojom::OnDataChangedEvent::Tag::kTab: {
      if (!data_changed_event->get_tab()) {
        return;
      }
      const Tab& updated = data_changed_event->get_tab()->data;
      if (Tab* existing = FindMutableTab(updated.id)) {
        *existing = updated;
        if (updated.is_active) {
          SetActiveTab(updated.id);
        } else if (active_tab_id_ == updated.id) {
          active_tab_id_.reset();
        }
      }
      break;
    }
    case mojom::OnDataChangedEvent::Tag::kTabGroup: {
      const auto& change = data_changed_event->get_tab_group();
      if (change && change->data) {
        if (mojom::Container* node = FindContainer(change->data->id)) {
          node->data = mojom::Data::NewTabGroup(change->data.Clone());
        }
      }
      break;
    }
    case mojom::OnDataChangedEvent::Tag::kSplitTab: {
      const auto& change = data_changed_event->get_split_tab();
      if (change && change->data) {
        if (mojom::Container* node = FindContainer(change->data->id)) {
          node->data = mojom::Data::NewSplitTab(change->data.Clone());
        }
      }
      break;
    }
  }
}

void TabStripServiceView::OnCollectionCreated(
    const mojom::OnCollectionCreatedEventPtr& collection_created_event) {
  if (!collection_created_event || !collection_created_event->collection) {
    return;
  }
  mojom::Container* parent =
      ResolveParentContainer(collection_created_event->position);
  if (!parent) {
    return;
  }
  ordered_tabs_.clear();
  IndexSubtree(
      InsertChildAt(*parent, collection_created_event->position.index(),
                    collection_created_event->collection.Clone()),
      parent);
  RebuildOrderedTabs();
}

void TabStripServiceView::OnTabStripServiceDestroyed() {
  observation_.Reset();
}

Tab* TabStripServiceView::FindMutableTab(const NodeId& id) {
  mojom::Container* node = FindContainer(id);
  return (node && node->data && node->data->is_tab()) ? &node->data->get_tab()
                                                      : nullptr;
}

mojom::Container* TabStripServiceView::FindContainer(const NodeId& id) {
  auto it = nodes_by_id_.find(id);
  return it != nodes_by_id_.end() ? it->second.container.get() : nullptr;
}

void TabStripServiceView::SetActiveTab(const NodeId& id) {
  if (active_tab_id_ && *active_tab_id_ != id) {
    if (Tab* prev = FindMutableTab(*active_tab_id_)) {
      prev->is_active = false;
    }
  }
  active_tab_id_ = id;
}

mojom::Container* TabStripServiceView::ResolveParentContainer(
    const Position& position) {
  const auto& components = position.path().components();
  if (!components.empty()) {
    if (mojom::Container* found = FindContainer(components.back())) {
      return found;
    }
  }
  if (!root_) {
    return nullptr;
  }
  if (root_->data && root_->data->is_window()) {
    for (const auto& child : root_->children) {
      if (child && child->data && child->data->is_tab_strip()) {
        return child.get();
      }
    }
  }
  return root_.get();
}

void TabStripServiceView::IndexSubtree(mojom::Container* container,
                                       mojom::Container* parent) {
  if (!container) {
    return;
  }
  if (container->data) {
    const NodeId& id = utils::GetNodeId(*container->data);
    if (auto existing = nodes_by_id_.find(id);
        existing != nodes_by_id_.end() &&
        existing->second.container != container) {
      mojom::Container* stale = existing->second.container;
      mojom::Container* stale_parent = existing->second.parent;
      UnindexSubtree(stale);
      if (stale_parent) {
        std::erase_if(
            stale_parent->children,
            [stale](const mojom::ContainerPtr& c) { return c.get() == stale; });
      }
    }
    nodes_by_id_[id] = NodeEntry{.container = container, .parent = parent};
    if (container->data->is_tab() && container->data->get_tab().is_active) {
      SetActiveTab(id);
    }
  }
  for (const auto& child : container->children) {
    IndexSubtree(child.get(), container);
  }
}

void TabStripServiceView::UnindexSubtree(mojom::Container* container) {
  if (!container) {
    return;
  }
  for (const auto& child : container->children) {
    UnindexSubtree(child.get());
  }
  if (container->data) {
    const NodeId& id = utils::GetNodeId(*container->data);
    if (active_tab_id_ == id) {
      active_tab_id_.reset();
    }
    nodes_by_id_.erase(id);
  }
}

void TabStripServiceView::RebuildOrderedTabs() {
  ordered_tabs_.clear();
  if (root_) {
    AppendTabs(*root_, ordered_tabs_);
  }
}

}  // namespace tabs_api
