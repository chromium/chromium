// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection.h"

#include <optional>
#include <set>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/tabs/public/supports_handles.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

DEFINE_HANDLE_FACTORY(TabCollection);

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

TabCollection::TabCollection(
    Type type,
    std::unordered_set<Type> supported_child_collections,
    bool supports_tabs,
    bool send_notifications_immediately)
    : type_(type),
      supported_child_collections_(supported_child_collections),
      supports_tabs_{supports_tabs},
      notify_immediately_{send_notifications_immediately},
      impl_(std::make_unique<TabCollectionStorage>(*this)) {}

TabCollection::~TabCollection() {
  DispatchPendingNotifications();
}

void TabCollection::AddObserver(TabCollectionObserver* observer) const {
  observers_.AddObserver(observer);
}

void TabCollection::RemoveObserver(TabCollectionObserver* observer) const {
  observers_.RemoveObserver(observer);
}

bool TabCollection::HasObserver(TabCollectionObserver* observer) const {
  return observers_.HasObserver(observer);
}

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

std::optional<size_t>
TabCollection::GetDirectChildIndexOfCollectionContainingTab(
    const TabInterface* tab) const {
  CHECK(tab);
  if (tab->GetParentCollection(GetPassKey()) == this) {
    return GetIndexOfTab(tab);
  } else {
    TabCollection* parent_collection = tab->GetParentCollection(GetPassKey());
    while (parent_collection && !ContainsCollection(parent_collection)) {
      parent_collection = parent_collection->GetParentCollection();
    }

    return GetIndexOfCollection(parent_collection);
  }
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

std::optional<TabCollection::Position> TabCollection::FindMovePositionRecursive(
    size_t destination_index,
    TabCollection* dst_collection,
    size_t& curr_insertion_index,
    const std::set<tabs::TabInterface*>& tabs_moved,
    const std::set<tabs::TabCollection*>& collections_moved) {
  size_t direct_child_index = 0;

  // Recursively find which position should the first tab_or_collection that is
  // being moved should go to. This increments `curr_index` only if the node is
  // not part of the nodes being moved.
  for (const auto& child : impl_->GetChildren()) {
    // Should not reach this state as it means the move position for the
    // operation does not exist.
    CHECK(curr_insertion_index <= destination_index)
        << " Could not find a move position "
        << " Current index: " << curr_insertion_index
        << " Destination to index: " << destination_index;
    if (curr_insertion_index == destination_index && this == dst_collection) {
      return TabCollection::Position(this->GetHandle(), direct_child_index);
    }
    if (std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(child)) {
      tabs::TabInterface* tab =
          std::get<std::unique_ptr<tabs::TabInterface>>(child).get();
      if (!tabs_moved.contains(tab)) {
        curr_insertion_index++;
      }
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      tabs::TabCollection* collection =
          std::get<std::unique_ptr<tabs::TabCollection>>(child).get();
      if (!collections_moved.contains(collection)) {
        // Recursively call into the collection.
        std::optional<TabCollection::Position> move_position =
            collection->FindMovePositionRecursive(
                destination_index, dst_collection, curr_insertion_index,
                tabs_moved, collections_moved);
        if (move_position.has_value()) {
          return move_position.value();
        }
      }
    }
    direct_child_index++;
  }

  // Case when we want to move to the end of this collection as a direct
  // child. This could also be a valid position.
  if (curr_insertion_index == destination_index && this == dst_collection) {
    return TabCollection::Position(this->GetHandle(), direct_child_index);
  }

  return std::nullopt;
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

void TabCollection::NotifyOnChildrenAdded(base::PassKey<TabCollection> pass_key,
                                          const TabCollectionNodes& handles,
                                          const Position& insertion_position,
                                          TabCollection* stop_notification_root,
                                          bool insert_from_detached) {
  if (stop_notification_root != nullptr && stop_notification_root == this) {
    return;
  }

  if (notify_immediately_) {
    observers_.Notify(&TabCollectionObserver::OnChildrenAdded,
                      insertion_position, handles, insert_from_detached);
  } else if (!observers_.empty()) {
    pending_notifications_.push_back(base::BindOnce(
        [](base::ObserverList<TabCollectionObserver>& observers,
           const Position& position, const TabCollectionNodes& handles,
           bool insert_from_detached) {
          observers.Notify(&TabCollectionObserver::OnChildrenAdded, position,
                           handles, insert_from_detached);
        },
        std::ref(observers_), insertion_position, handles,
        insert_from_detached));
  }

  if (parent_) {
    parent_->NotifyOnChildrenAdded(pass_key, handles, insertion_position,
                                   stop_notification_root,
                                   insert_from_detached);
  }
}

void TabCollection::NotifyOnChildrenRemoved(
    base::PassKey<TabCollection> pass_key,
    const Position& position,
    const TabCollectionNodes& handles,
    TabCollection* stop_notification_root) {
  if (stop_notification_root != nullptr && stop_notification_root == this) {
    return;
  }

  if (notify_immediately_) {
    observers_.Notify(&TabCollectionObserver::OnChildrenRemoved, position,
                      handles);
  } else if (!observers_.empty()) {
    pending_notifications_.push_back(base::BindOnce(
        [](const Position& position,
           base::ObserverList<TabCollectionObserver>& observers,
           const TabCollectionNodes& handles) {
          observers.Notify(&TabCollectionObserver::OnChildrenRemoved, position,
                           handles);
        },
        std::ref(position), std::ref(observers_), handles));
  }

  if (parent_) {
    parent_->NotifyOnChildrenRemoved(pass_key, position, handles,
                                     stop_notification_root);
  }
}

void TabCollection::NotifyOnChildMoved(base::PassKey<TabCollection> pass_key,
                                       const TabCollectionNodeHandle& handle,
                                       const Position& src_position,
                                       const Position& dst_position,
                                       TabCollection* stop_notification_root) {
  if (stop_notification_root != nullptr && stop_notification_root == this) {
    return;
  }

  TabCollectionObserver::NodeData src_data =
      TabCollectionObserver::NodeData(src_position, handle);

  if (notify_immediately_) {
    observers_.Notify(&TabCollectionObserver::OnChildMoved, dst_position,
                      src_data);
  } else if (!observers_.empty()) {
    pending_notifications_.push_back(base::BindOnce(
        [](base::ObserverList<TabCollectionObserver>& observers,
           const Position& dst_position,
           const TabCollectionObserver::NodeData& src_data) {
          observers.Notify(&TabCollectionObserver::OnChildMoved, dst_position,
                           src_data);
        },
        std::ref(observers_), dst_position, src_data));
  }

  if (parent_) {
    parent_->NotifyOnChildMoved(pass_key, handle, src_position, dst_position,
                                stop_notification_root);
  }
}

void TabCollection::DispatchPendingNotifications() {
  for (auto& notification : pending_notifications_) {
    std::move(notification).Run();
  }

  pending_notifications_.clear();
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

const ChildrenVector& TabCollection::GetChildren(
    base::PassKey<DirectChildWalker> pass_key) const {
  return GetChildren();
}
}  // namespace tabs
