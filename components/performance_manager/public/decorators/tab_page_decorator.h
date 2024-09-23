// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PAGE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PAGE_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

class TabPageObserver;

// A decorator that adorns `PageNode`s with some data that allows it to provide
// a stable handle to a tab, even though the underlying `PageNode` associated
// with that tab might change due to tab discarding. Registering as an observer
// or using the `FromPageNode` function allows callers to obtain a pointer to a
// `TabHandle` object whose `page_node()` function always points to the
// `PageNode` associated with the tab.
class TabPageDecorator : public PageNode::ObserverDefaultImpl,
                         public GraphOwnedAndRegistered<TabPageDecorator> {
 public:
  class Data;
  class TabHandle;

  TabPageDecorator();
  ~TabPageDecorator() override;

  void AddObserver(TabPageObserver* observer);
  void RemoveObserver(TabPageObserver* observer);

  // Returns the `TabHandle` associated with `page_node`, if `page_node` is a
  // tab. Returns nullptr if `page_node` is not a tab.
  static TabPageDecorator::TabHandle* FromPageNode(const PageNode* page_node);

 private:
  void MaybeTabCreated(const PageNode* page_node);

  // PageNode::ObserverDefaultImpl:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnAboutToBeDiscarded(const PageNode* page_node,
                            const PageNode* new_page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  base::ObserverList<TabPageObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// A stable handle to a tab. Objects of this class are created when a `PageNode`
// becomes a tab, and are only destroyed when that tab is closed. During tab
// discarding, the `PageNode` returned by `page_node()` will change. Callers may
// store pointers to `TabHandle` objects until
// `TabPageObserver::OnBeforeTabRemoved` is called for said `TabHandle` object,
// and they are guaranteed that `page_node()` will return the `PageNode`
// associated with the tab.
class TabPageDecorator::TabHandle {
 public:
  const PageNode* page_node() const { return page_node_; }

 private:
  friend class TabPageDecorator::Data;
  friend class TabPageDecorator;

  explicit TabHandle(const PageNode* page_node) : page_node_(page_node) {}

  void SetPageNode(const PageNode* page_node) { page_node_ = page_node; }

  raw_ptr<const PageNode> page_node_;
};

class TabPageObserver : public base::CheckedObserver {
 public:
  // Called when a `PageNode` is given the `kTab` type, and right after
  // `tab_handle` is created for this `PageNode`.
  virtual void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) = 0;
  // Called before `old_page_node` is deleted, but after `tab_handle`'s
  // underlying `page_node()` is changed to the new `PageNode` associated with
  // the tab. `page_node()` may still be of type `kUnknown` at this point.
  virtual void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) = 0;
  // Called right before the tab is closed and its `PageNode` destroyed.
  // `tab_handle` is invalid after this function has been invoked.
  virtual void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) = 0;
};

class TabPageObserverDefaultImpl : public TabPageObserver {
 public:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override {}
  void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) override {}
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PAGE_DECORATOR_H_
