// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// This does not create a useful iterator, but providing a default constructor
// is required for forward iterators by the C++ spec.
TabCollection::TabIterator::TabIterator() : TabIterator(nullptr, true) {}

TabCollection::TabIterator::TabIterator(base::PassKey<TabCollection>,
                                        const TabCollection* root,
                                        bool is_end)
    : TabIterator(root, is_end) {}

TabCollection::TabIterator::TabIterator(const tabs::TabCollection* root,
                                        bool is_end)
    : cur_(nullptr), root_(root) {
  if (!is_end && root) {
    stack_.reserve(10);
    stack_.push_back({root, 0});
    Next();
  }
}

TabCollection::TabIterator::TabIterator(const TabIterator& iterator) = default;

TabCollection::TabIterator::~TabIterator() = default;

void TabCollection::TabIterator::Next() {
  DCHECK(cur_ || !stack_.empty()) << "Trying to advance past the end";
  cur_ = nullptr;
  while (!stack_.empty()) {
    // Copy by reference to update the index below.
    Frame& frame = stack_[stack_.size() - 1];
    const TabCollection* collection = frame.collection;

    const auto& children = collection->GetChildren();

    if (frame.index < children.size()) {
      auto& child = children[frame.index++];
      if (std::holds_alternative<std::unique_ptr<TabInterface>>(child)) {
        cur_ = std::get<std::unique_ptr<TabInterface>>(child).get();
        return;
      } else {
        TabCollection* child_collection =
            std::get<std::unique_ptr<TabCollection>>(child).get();
        // Optimization for `pinned_collection_` which can exist even without
        // any children.
        if (child_collection->ChildCount() > 0) {
          stack_.push_back({child_collection, 0});
        }
      }
    } else {
      stack_.pop_back();
    }
  }
}

TabCollection::Iterator::Iterator()
    : root_collection_(nullptr), is_end_iterator_(true) {}

TabCollection::Iterator::Iterator(const Iterator& other) = default;
TabCollection::Iterator::~Iterator() = default;

TabCollection::Iterator::Iterator(base::PassKey<TabCollection>,
                                  const TabCollection* root,
                                  bool is_end /*= false*/)
    : root_collection_(root) {
  if (is_end || !root) {
    cur_ = static_cast<const TabCollection*>(nullptr);
    is_end_iterator_ = true;
    if (!root) {
      root_collection_ = nullptr;
    }
  } else {
    cur_ = root;
    is_end_iterator_ = false;
    stack_.push_back({root, 0});
  }
}

void TabCollection::Iterator::Advance() {
  if (stack_.empty()) {
    cur_ = static_cast<const TabCollection*>(nullptr);
    is_end_iterator_ = true;
    return;
  }

  Frame* current_frame = &stack_.back();
  const auto& children = current_frame->collection->GetChildren();

  if (current_frame->child_idx < children.size()) {
    const auto& child_variant = children[current_frame->child_idx];
    current_frame->child_idx++;

    if (std::holds_alternative<std::unique_ptr<TabInterface>>(child_variant)) {
      cur_ = std::get<std::unique_ptr<TabInterface>>(child_variant).get();
      return;
    } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(
                   child_variant)) {
      TabCollection* sub_collection =
          std::get<std::unique_ptr<TabCollection>>(child_variant).get();
      stack_.push_back({sub_collection, 0});
      cur_ = sub_collection;
      return;
    }
  } else {
    stack_.pop_back();
    Advance();
  }
}

TabCollection::TabCollection(
    Type type,
    std::unordered_set<Type> supported_child_collections,
    bool supports_tabs)
    : type_(type),
      supported_child_collections_(supported_child_collections),
      supports_tabs_{supports_tabs},
      impl_(std::make_unique<TabCollectionStorage>(*this)) {}

TabCollection::~TabCollection() = default;

bool TabCollection::ContainsCollection(TabCollection* collection) const {
  CHECK(collection);
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> TabCollection::GetIndexOfTab(
    const TabInterface* tab) const {
  CHECK(tab);
  return impl_->GetIndexOfTab(tab);
}

std::optional<size_t> TabCollection::GetIndexOfTabRecursive(
    const TabInterface* tab) const {
  CHECK(tab);
  size_t current_index = 0;

  // If the child is a `TabInterface` check if it is the the desired tab,
  // otherwise increase the current_index by 1.
  // Otherwise the child is a collection. If the tab is present in the
  // collection, use the relative index and the `current_index` and return the
  // result. Otherwise, update the `current_index` by the number of tabs in the
  // collection.
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<TabInterface>>(child)) {
      if (std::get<std::unique_ptr<TabInterface>>(child).get() == tab) {
        return current_index;
      }
      current_index++;
    } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(child)) {
      const TabCollection* const collection =
          std::get<std::unique_ptr<TabCollection>>(child).get();

      if (std::optional<size_t> index_within_collection =
              collection->GetIndexOfTabRecursive(tab);
          index_within_collection.has_value()) {
        return current_index + index_within_collection.value();
      } else {
        current_index += collection->TabCountRecursive();
      }
    }
  }

  return std::nullopt;
}

TabInterface* TabCollection::GetTabAtIndexRecursive(size_t index) const {
  size_t curr_index = 0;

  for (auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<TabInterface>>(child)) {
      if (curr_index == index) {
        return std::get<std::unique_ptr<TabInterface>>(child).get();
      } else {
        curr_index++;
      }
    } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(child)) {
      TabCollection* collection =
          std::get<std::unique_ptr<TabCollection>>(child).get();
      size_t num_of_tabs_in_sub_collection = collection->TabCountRecursive();

      if (index < curr_index + num_of_tabs_in_sub_collection) {
        return collection->GetTabAtIndexRecursive(index - curr_index);
      } else {
        curr_index += num_of_tabs_in_sub_collection;
      }
    }
  }
  NOTREACHED();
}

std::vector<TabInterface*> TabCollection::GetTabsRecursive() const {
  std::vector<TabInterface*> tabs;
  tabs.reserve(TabCountRecursive());
  for (tabs::TabInterface* tab : *this) {
    tabs.push_back(tab);
  }

  return tabs;
}

std::optional<size_t> TabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return impl_->GetIndexOfCollection(collection);
}

size_t TabCollection::ToDirectIndex(size_t index) {
  CHECK(index <= TabCountRecursive());

  size_t curr_index = 0;
  size_t direct_child_index = 0;
  for (const auto& child : impl_->GetChildren()) {
    CHECK(curr_index <= index);
    if (curr_index == index) {
      return direct_child_index;
    }
    if (std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(child)) {
      curr_index++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      curr_index += std::get<std::unique_ptr<tabs::TabCollection>>(child)
                        ->TabCountRecursive();
    }
    direct_child_index++;
  }

  CHECK(curr_index == index);
  CHECK(direct_child_index == ChildCount());
  return direct_child_index;
}

size_t TabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

void TabCollection::OnCollectionAddedToTree(TabCollection* collection) {
  recursive_tab_count_ += collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionAddedToTree(collection);
  }
}

void TabCollection::OnCollectionRemovedFromTree(TabCollection* collection) {
  recursive_tab_count_ -= collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionRemovedFromTree(collection);
  }
}

void TabCollection::OnTabAddedToTree() {
  recursive_tab_count_++;

  if (parent_) {
    parent_->OnTabAddedToTree();
  }
}

void TabCollection::OnTabRemovedFromTree() {
  recursive_tab_count_--;

  if (parent_) {
    parent_->OnTabRemovedFromTree();
  }
}

TabInterface* TabCollection::AddTab(std::unique_ptr<TabInterface> tab,
                                    size_t index) {
  CHECK(tab);
  CHECK(supports_tabs_);

  TabInterface* inserted_tab = impl_->AddTab(std::move(tab), index);
  inserted_tab->OnReparented(this, GetPassKey());
  return inserted_tab;
}

void TabCollection::MoveTab(TabInterface* tab, size_t index) {
  CHECK(tab);

  impl_->MoveTab(tab, index);
}

std::unique_ptr<TabInterface> TabCollection::MaybeRemoveTab(TabInterface* tab) {
  CHECK(tab);

  std::unique_ptr<TabInterface> removed_tab = impl_->RemoveTab(tab);
  removed_tab->OnReparented(nullptr, GetPassKey());
  return removed_tab;
}

std::unique_ptr<TabCollection> TabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  CHECK(collection);

  std::unique_ptr<TabCollection> removed_tab_collection =
      impl_->RemoveCollection(collection);
  removed_tab_collection->OnReparented(nullptr);
  return removed_tab_collection;
}

void TabCollection::OnReparented(TabCollection* new_parent) {
  parent_ = new_parent;

  for (auto tab : GetTabsRecursive()) {
    tab->OnAncestorChanged(GetPassKey());
  }
}

}  // namespace tabs
