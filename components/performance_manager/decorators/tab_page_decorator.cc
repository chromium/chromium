// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_page_decorator.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "content/public/common/content_features.h"

namespace performance_manager {

class TabPageDecorator::Data
    : public ExternalNodeAttachedDataImpl<TabPageDecorator::Data> {
 public:
  explicit Data(const PageNodeImpl* page_node)
      : tab_handle_(base::WrapUnique(new TabHandle(page_node))) {}

  TabHandle* tab_handle() const { return tab_handle_.get(); }

  void TransferTabHandleTo(TabPageDecorator::Data* target) {
    target->tab_handle_ = std::move(tab_handle_);
  }

 private:
  std::unique_ptr<TabHandle> tab_handle_;
};

TabPageDecorator::TabPageDecorator() = default;
TabPageDecorator::~TabPageDecorator() = default;

void TabPageDecorator::AddObserver(TabPageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void TabPageDecorator::RemoveObserver(TabPageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

// static
TabPageDecorator::TabHandle* TabPageDecorator::FromPageNode(
    const PageNode* page_node) {
  TabPageDecorator::Data* data =
      TabPageDecorator::Data::Get(PageNodeImpl::FromNode(page_node));
  if (!data) {
    return nullptr;
  }

  return data->tab_handle();
}

void TabPageDecorator::MaybeTabCreated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_node->GetType() != performance_manager::PageType::kTab) {
    return;
  }

  TabPageDecorator::Data* data =
      TabPageDecorator::Data::Get(PageNodeImpl::FromNode(page_node));

  // If this PageNode already has a `TabPageDecorator::Data` attached, it means
  // that it's a new PageNode created as part of the discard process. It was
  // already changed to point to the correct PageNode in `AboutToBeDiscarded`.
  // Because it's not a new tab, don't notify observers of tab creation.
  if (data) {
    return;
  }

  // This is a new tab, create its `TabPageDecorator::Data` and notify
  // observers.
  data = TabPageDecorator::Data::GetOrCreate(PageNodeImpl::FromNode(page_node));

  for (auto& obs : observers_) {
    obs.OnTabAdded(data->tab_handle());
  }
}

// PageNode::ObserverDefaultImpl:
void TabPageDecorator::OnPageNodeAdded(const PageNode* page_node) {
  MaybeTabCreated(page_node);
}

void TabPageDecorator::OnBeforePageNodeRemoved(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Try to get the data. If it exists, it means this was a tab and we should
  // notify that it's being removed. If it doesn't exist, that means that this
  // node was either not a tab, or it's a node associated with a tab that was
  // just discarded. In this case, we don't notify callers of the removal since
  // the tab still exists, it's just its underlying PageNode that changed.
  TabPageDecorator::Data* data =
      TabPageDecorator::Data::Get(PageNodeImpl::FromNode(page_node));

  if (data) {
    for (auto& obs : observers_) {
      obs.OnBeforeTabRemoved(data->tab_handle());
    }
  }
}

void TabPageDecorator::OnAboutToBeDiscarded(const PageNode* page_node,
                                            const PageNode* new_page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(page_node->GetType(), performance_manager::PageType::kTab);

  // When kWebContentsDiscard is disabled new_page_node will be different from
  // page node and needs handling to transfer data from the old node.
  if (base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    CHECK_EQ(page_node, new_page_node);
  } else {
    CHECK_EQ(new_page_node->GetType(), performance_manager::PageType::kUnknown);
    // Move the handle out of the old node's data into the new one. We do this
    // so that users of the API can keep using the same TabHandle object
    // transparently regardless of whether the underlying PageNode changes.
    TabPageDecorator::Data* old_data =
        TabPageDecorator::Data::Get(PageNodeImpl::FromNode(page_node));

    // The new PageNode is created with kUnknown type, so create the data for it
    // here. This will also prevent observers from being notified of tab
    // creation when this PageNode's type changes to kTab.
    CHECK(!TabPageDecorator::Data::Get(PageNodeImpl::FromNode(new_page_node)));
    TabPageDecorator::Data* new_data = TabPageDecorator::Data::GetOrCreate(
        PageNodeImpl::FromNode(new_page_node));

    CHECK(old_data);

    old_data->tab_handle()->SetPageNode(new_page_node);
    old_data->TransferTabHandleTo(new_data);

    // Destroy the old node's data so OnBeforePageNodeRemoved doesn't treat it
    // as a tab going away
    bool destroyed =
        TabPageDecorator::Data::Destroy(PageNodeImpl::FromNode(page_node));
    CHECK(destroyed);
  }

  for (auto& obs : observers_) {
    obs.OnTabAboutToBeDiscarded(
        page_node,
        TabPageDecorator::Data::Get(PageNodeImpl::FromNode(new_page_node))
            ->tab_handle());
  }
}

void TabPageDecorator::OnTypeChanged(const PageNode* page_node,
                                     PageType previous_type) {
  // Once a PageNode is a tab, it can't transform into anything else.
  CHECK_NE(previous_type, performance_manager::PageType::kTab);

  MaybeTabCreated(page_node);
}

void TabPageDecorator::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
}

void TabPageDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
}

}  // namespace performance_manager
