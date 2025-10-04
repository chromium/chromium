// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_util.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace tab_strip_internals {

namespace {

// Returns the root TabCollection for a given tab.
const tabs::TabCollection* GetRootCollectionForTab(
    const tabs::TabInterface* tab) {
  const tabs::TabCollection* current = tab->GetParentCollection();
  while (current && current->GetParentCollection()) {
    current = current->GetParentCollection();
  }
  return current;
}

// Build a MojoTabCollection Node from a given TabCollection.
// i.e. maps a TabCollection to a mojo::DataPtr.
mojom::DataPtr BuildMojoCollection(const tabs::TabCollection* collection) {
  auto node_id = tab_strip_internals::MakeNodeId(
      base::NumberToString(collection->GetHandle().raw_value()),
      mojom::NodeId::Type::kCollection);

  switch (collection->type()) {
    case tabs::TabCollection::Type::TABSTRIP: {
      auto tabstrip = mojom::TabStripCollection::New();
      tabstrip->id = std::move(node_id);
      return mojom::Data::NewTabStripCollection(std::move(tabstrip));
    }
    case tabs::TabCollection::Type::PINNED: {
      auto pinned_tabs = mojom::PinnedCollection::New();
      pinned_tabs->id = std::move(node_id);
      return mojom::Data::NewPinnedTabCollection(std::move(pinned_tabs));
    }
    case tabs::TabCollection::Type::UNPINNED: {
      auto unpinned_tabs = mojom::UnpinnedCollection::New();
      unpinned_tabs->id = std::move(node_id);
      return mojom::Data::NewUnpinnedTabCollection(std::move(unpinned_tabs));
    }
    case tabs::TabCollection::Type::GROUP: {
      auto group_tabs = mojom::GroupCollection::New();
      group_tabs->id = std::move(node_id);

      const auto* group_collection =
          static_cast<const tabs::TabGroupTabCollection*>(collection);
      const TabGroup* tab_group = group_collection->GetTabGroup();
      if (tab_group) {
        group_tabs->visualData = mojom::TabGroupVisualData::New();
        group_tabs->visualData->title =
            base::UTF16ToUTF8(tab_group->visual_data()->title());
        group_tabs->visualData->color = tab_group->visual_data()->color();
        group_tabs->visualData->is_collapsed =
            tab_group->visual_data()->is_collapsed();
      }
      return mojom::Data::NewTabGroupCollection(std::move(group_tabs));
    }
    case tabs::TabCollection::Type::SPLIT: {
      auto split_tabs = mojom::SplitCollection::New();
      split_tabs->id = std::move(node_id);

      const auto* split_collection =
          static_cast<const tabs::SplitTabCollection*>(collection);
      if (auto* split_tab_data = split_collection->data()) {
        if (auto* visual_data = split_tab_data->visual_data()) {
          split_tabs->visualData = mojom::SplitTabVisualData::New();
          split_tabs->visualData->layout =
              static_cast<mojom::SplitTabVisualData::Layout>(
                  static_cast<int>(visual_data->split_layout()));
          split_tabs->visualData->split_ratio = visual_data->split_ratio();
        }
      }
      return mojom::Data::NewSplitTabCollection(std::move(split_tabs));
    }
    default:
      NOTREACHED();
  }
}

// Build a MojoTab Node from a given TabInterface.
// i.e. maps a TabInterface to a mojo::DataPtr.
mojom::DataPtr BuildMojoTab(const tabs::TabInterface* tab) {
  auto node_id = tab_strip_internals::MakeNodeId(
      base::NumberToString(tab->GetHandle().raw_value()),
      mojom::NodeId::Type::kTab);

  auto mojo_tab = mojom::Tab::New();
  mojo_tab->id = std::move(node_id);

  content::WebContents* data = tab->GetContents();
  if (data) {
    mojo_tab->title = base::UTF16ToUTF8(data->GetTitle());
    mojo_tab->url = data->GetVisibleURL();
  }

  return mojom::Data::NewTab(std::move(mojo_tab));
}

}  // namespace

mojom::NodeIdPtr MakeNodeId(const std::string& id, mojom::NodeId::Type type) {
  return mojom::NodeId::New(id, type);
}

mojom::NodePtr BuildTabCollectionTree(const TabStripModel* model) {
  if (!model || model->empty()) {
    return mojom::Node::New();
  }

  auto* root_collection = GetRootCollectionForTab(model->GetTabAtIndex(0));
  if (!root_collection) {
    return mojom::Node::New();
  }

  std::unordered_map<const tabs::TabCollection*, mojom::Node*> map_collection;

  auto root_node = mojom::Node::New();
  root_node->data = BuildMojoCollection(root_collection);
  map_collection[root_collection] = root_node.get();

  // Iterate all leaf nodes (tabs) in the tabstrip tree.
  // TODO (crbug.com/427204855): Look into providing a Passkey for
  // TabCollection::GetChildren to allow top-down traversal.
  for (size_t i = 0; i < root_collection->TabCountRecursive(); i++) {
    const tabs::TabInterface* tab = root_collection->GetTabAtIndexRecursive(i);
    if (!tab) {
      continue;
    }

    // Build a chain of parents tracing back to the root collection in a
    // bottom-up fashion.
    const tabs::TabCollection* parent = tab->GetParentCollection();
    // Entries will be added left to right, so root will be the last entry.
    std::vector<const tabs::TabCollection*> chain;
    while (parent) {
      chain.push_back(parent);
      parent = parent->GetParentCollection();
    }

    // Walk the chain in a top-down fashion.
    mojom::Node* parent_node = root_node.get();
    for (auto collection : base::Reversed(chain)) {
      mojom::Node*& child_node = map_collection[collection];

      if (!child_node) {
        auto new_node = mojom::Node::New();
        new_node->data = BuildMojoCollection(collection);

        child_node = new_node.get();
        // Link the new node to its parent.
        parent_node->children.push_back(std::move(new_node));
      }

      // Move down to the next level.
      parent_node = child_node;
    }

    // Attach the leaf node (a tab).
    auto tab_node = mojom::Node::New();
    tab_node->data = BuildMojoTab(tab);
    parent_node->children.push_back(std::move(tab_node));
  }

  return root_node;
}

mojom::SelectionModelPtr BuildSelectionModel(const TabStripModel* model) {
  if (!model || model->empty()) {
    return mojom::SelectionModel::New();
  }

  auto mojo_sel_model = tab_strip_internals::mojom::SelectionModel::New();
  mojo_sel_model->active_index = model->active_index();

  const ui::ListSelectionModel& sel_model = model->selection_model();
  // Below logic does not exist in TabStripModel, so we build it here.
  mojo_sel_model->anchor_index =
      sel_model.anchor().has_value() ? *sel_model.anchor() : -1;

  for (size_t idx : sel_model.selected_indices()) {
    mojo_sel_model->selected_indices.push_back(idx);
  }

  return mojo_sel_model;
}

}  // namespace tab_strip_internals
