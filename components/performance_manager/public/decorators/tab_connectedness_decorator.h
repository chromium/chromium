// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_CONNECTEDNESS_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_CONNECTEDNESS_DECORATOR_H_

#include "base/observer_list_types.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"

namespace performance_manager {

// This decorator creates and maintains a piece of NodeAttachedData that tracks
// information related to how connected a tab is to other tabs. Connectedness
// from tab A to tab B is defined as the probability that the user will reach
// tab B from tab A by switching tabs in a manner that is consistent with their
// previous tab switches in the current session.
// Connectedness is:
//   - undirected, so C(a, b) != C(b, a)
//   - independent of the tab's origin
// For a specific path from A to B, it is computed by taking the product of the
// weight of all edges along the path. The overall connectedness is the sum of
// the individual scores for all paths leading from A to B.
class TabConnectednessDecorator
    : public GraphOwnedDefaultImpl,
      public GraphRegisteredImpl<TabConnectednessDecorator>,
      public TabPageObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnBeforeTabSwitch(
        TabPageDecorator::TabHandle* source,
        TabPageDecorator::TabHandle* destination) = 0;
  };

  TabConnectednessDecorator();
  ~TabConnectednessDecorator() override;

  // Recursively computes the connectedness from `source` to `destination`.
  static float ComputeConnectednessBetween(
      TabPageDecorator::TabHandle* source,
      TabPageDecorator::TabHandle* destination);

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Used from the UI thread to notify this decorator that tab `from` is
  // becoming inactive and `to` is becoming active.
  static void NotifyOfTabSwitch(content::WebContents* from,
                                content::WebContents* to);

 private:
  friend class TabConnectednessDecoratorTest;
  friend class TabRevisitTrackerTest;

  void OnTabSwitch(const PageNode* from, const PageNode* to);

  // TabPageObserver:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_CONNECTEDNESS_DECORATOR_H_
