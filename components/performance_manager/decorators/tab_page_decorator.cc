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
  CHECK_EQ(page_node, new_page_node);
  CHECK_EQ(page_node->GetType(), performance_manager::PageType::kTab);

  // TODO(crbug.com/347770670): Remove this now that new page nodes are no
  // longer created during discard operations.
  TabPageDecorator::Data* old_data =
      TabPageDecorator::Data::Get(PageNodeImpl::FromNode(page_node));

  for (auto& obs : observers_) {
    obs.OnTabAboutToBeDiscarded(page_node, old_data->tab_handle());
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
