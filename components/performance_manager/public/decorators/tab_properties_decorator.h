// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PROPERTIES_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PROPERTIES_DECORATOR_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PageNode;

// The TabProperties decorator is responsible for tracking properties of
// PageNodes that are tabs. All the functions that take a WebContents* as a
// parameter should only be called from the UI thread, the event will be
// forwarded to the corresponding PageNode on the Performance Manager's
// sequence.
class TabPropertiesDecorator : public GraphOwned,
                               public NodeDataDescriberDefaultImpl {
 public:
  class Data;

  // This object should only be used via its static methods.
  TabPropertiesDecorator() = default;
  ~TabPropertiesDecorator() override = default;
  TabPropertiesDecorator(const TabPropertiesDecorator& other) = delete;
  TabPropertiesDecorator& operator=(const TabPropertiesDecorator&) = delete;

  // Set the is_tab property of a PageNode.
  static void SetIsTab(content::WebContents* contents, bool is_tab);

  static void SetIsTabForTesting(PageNode* page_node, bool is_tab);

 private:
  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;
};

class TabPropertiesDecorator::Data {
 public:
  Data();
  virtual ~Data();
  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  // Indicates if a PageNode belongs to a tab strip.
  virtual bool IsInTabStrip() const = 0;

  static const Data* FromPageNode(const PageNode* page_node);
  static Data* GetOrCreateForTesting(const PageNode* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_TAB_PROPERTIES_DECORATOR_H_
