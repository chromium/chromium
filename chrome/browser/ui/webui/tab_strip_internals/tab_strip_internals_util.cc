// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_util.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_types.h"
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
  mojo_tab->active = tab->IsActivated();
  mojo_tab->visible = tab->IsVisible();
  mojo_tab->selected = tab->IsSelected();
  mojo_tab->pinned = tab->IsPinned();
  mojo_tab->split = tab->IsSplit();
  mojo_tab->alert_states =
      tabs::TabAlertController::From(tab)->GetAllActiveAlerts();

  return mojom::Data::NewTab(std::move(mojo_tab));
}

// Build a TabRestoreEntryBase from an Entry.
mojom::TabRestoreEntryBasePtr BuildTabRestoreEntryBase(
    const sessions::tab_restore::Entry& entry) {
  auto base = mojom::TabRestoreEntryBase::New();
  base->original_id = entry.original_id.id();

  if (!entry.timestamp.is_null()) {
    base->timestamp = entry.timestamp;
  }
  return base;
}

// Build a single TabRestoreTab entry.
mojom::TabRestoreTabPtr BuildTabRestoreTab(
    const sessions::tab_restore::Tab& tab) {
  auto mojo_tab = mojom::TabRestoreTab::New();
  mojo_tab->id = MakeNodeId(base::NumberToString(tab.id.id()),
                            mojom::NodeId::Type::kTabRestoreTab);
  mojo_tab->restore_entry = BuildTabRestoreEntryBase(tab);
  mojo_tab->browser_id = tab.browser_id;
  mojo_tab->tabstrip_index = tab.tabstrip_index;
  mojo_tab->pinned = tab.pinned;

  if (!tab.navigations.empty()) {
    const sessions::SerializedNavigationEntry& nav =
        tab.navigations[tab.normalized_navigation_index()];
    mojo_tab->title = base::UTF16ToUTF8(nav.title());
    mojo_tab->url = nav.virtual_url();
  }

  if (tab.group.has_value()) {
    mojo_tab->group_id = tab.group->token();
  }

  if (tab.group_visual_data.has_value()) {
    const auto& data = *tab.group_visual_data;
    mojo_tab->group_visual_data = mojom::TabGroupVisualData::New(
        base::UTF16ToUTF8(data.title()), data.color(), data.is_collapsed());
  }

  return mojo_tab;
}

// Build a single TabRestoreGroup entry.
mojom::TabRestoreGroupPtr BuildTabRestoreGroup(
    const sessions::tab_restore::Group& group) {
  auto mojo_group = mojom::TabRestoreGroup::New();
  mojo_group->id = MakeNodeId(base::NumberToString(group.id.id()),
                              mojom::NodeId::Type::kTabRestoreGroup);
  mojo_group->restore_entry = BuildTabRestoreEntryBase(group);
  mojo_group->browser_id = group.browser_id;
  mojo_group->group_id = group.group_id.token();
  mojo_group->visual_data = mojom::TabGroupVisualData::New(
      base::UTF16ToUTF8(group.visual_data.title()), group.visual_data.color(),
      group.visual_data.is_collapsed());

  for (const std::unique_ptr<sessions::tab_restore::Tab>& tab : group.tabs) {
    mojo_group->tabs.push_back(BuildTabRestoreTab(*tab));
  }

  return mojo_group;
}

// Build a single TabRestoreWindow entry.
mojom::TabRestoreWindowPtr BuildTabRestoreWindow(
    const sessions::tab_restore::Window& window) {
  auto mojo_window = mojom::TabRestoreWindow::New();
  mojo_window->id = MakeNodeId(base::NumberToString(window.id.id()),
                               mojom::NodeId::Type::kTabRestoreWindow);
  mojo_window->restore_entry = BuildTabRestoreEntryBase(window);
  mojo_window->selected_tab_index = window.selected_tab_index;

  for (const std::unique_ptr<sessions::tab_restore::Tab>& tab : window.tabs) {
    mojo_window->tabs.push_back(BuildTabRestoreTab(*tab));
  }

  return mojo_window;
}

}  // namespace

mojom::NodeIdPtr MakeNodeId(const std::string& id, mojom::NodeId::Type type) {
  return mojom::NodeId::New(id, type);
}

mojom::NodePtr BuildTabCollectionTree(const TabStripModel* model) {
  CHECK(model);
  if (model->empty()) {
    return nullptr;
  }

  auto* root_collection = GetRootCollectionForTab(model->GetTabAtIndex(0));
  CHECK(root_collection);

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
  CHECK(model);
  if (model->empty()) {
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

mojom::TabRestoreDataPtr BuildTabRestoreData(
    const sessions::TabRestoreEntries& entries) {
  auto data = mojom::TabRestoreData::New();

  for (const auto& entry : entries) {
    mojom::TabRestoreEntryPtr mojo_entry;

    switch (entry->type) {
      case sessions::tab_restore::TAB: {
        const auto* tab =
            static_cast<const sessions::tab_restore::Tab*>(entry.get());
        mojo_entry = mojom::TabRestoreEntry::NewTab(BuildTabRestoreTab(*tab));
        break;
      }
      case sessions::tab_restore::WINDOW: {
        const auto* window =
            static_cast<const sessions::tab_restore::Window*>(entry.get());
        mojo_entry =
            mojom::TabRestoreEntry::NewWindow(BuildTabRestoreWindow(*window));
        break;
      }
      case sessions::tab_restore::GROUP: {
        const auto* group =
            static_cast<const sessions::tab_restore::Group*>(entry.get());
        mojo_entry =
            mojom::TabRestoreEntry::NewGroup(BuildTabRestoreGroup(*group));
        break;
      }
    }

    data->entries.push_back(std::move(mojo_entry));
  }

  return data;
}

}  // namespace tab_strip_internals
