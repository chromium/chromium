// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_properties_decorator.h"

#include "components/performance_manager/decorators/decorators_utils.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

class TabPropertiesDataImpl
    : public TabPropertiesDecorator::Data,
      public NodeAttachedDataImpl<TabPropertiesDataImpl> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
  ~TabPropertiesDataImpl() override = default;
  TabPropertiesDataImpl(const TabPropertiesDataImpl& other) = delete;
  TabPropertiesDataImpl& operator=(const TabPropertiesDataImpl&) = delete;

  // TabPropertiesDecorator::Data implementation.
  bool IsInTabStrip() const override { return is_tab_; }

  void set_is_tab(bool is_tab) { is_tab_ = is_tab; }

 private:
  // Make the impl our friend so it can access the constructor and any
  // storage providers.
  friend class ::performance_manager::NodeAttachedDataImpl<
      TabPropertiesDataImpl>;

  explicit TabPropertiesDataImpl(const PageNodeImpl* page_node) {}

  bool is_tab_ = false;
};

const char kDescriberName[] = "TabPropertiesDecorator";

}  // namespace

void TabPropertiesDecorator::SetIsTab(content::WebContents* contents,
                                      bool is_tab) {
  SetPropertyForWebContentsPageNode(contents,
                                    &TabPropertiesDataImpl::set_is_tab, is_tab);
}

void TabPropertiesDecorator::SetIsTabForTesting(PageNode* page_node,
                                                bool is_tab) {
  auto* data =
      TabPropertiesDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
  DCHECK(data);
  data->set_is_tab(is_tab);
}

void TabPropertiesDecorator::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void TabPropertiesDecorator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value TabPropertiesDecorator::DescribePageNodeData(
    const PageNode* node) const {
  auto* data = TabPropertiesDecorator::Data::FromPageNode(node);
  if (!data)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetBoolKey("IsInTabStrip", data->IsInTabStrip());

  return ret;
}

TabPropertiesDecorator::Data::Data() = default;
TabPropertiesDecorator::Data::~Data() = default;

const TabPropertiesDecorator::Data* TabPropertiesDecorator::Data::FromPageNode(
    const PageNode* page_node) {
  return TabPropertiesDataImpl::Get(PageNodeImpl::FromNode(page_node));
}

TabPropertiesDecorator::Data*
TabPropertiesDecorator::Data::GetOrCreateForTesting(const PageNode* page_node) {
  return TabPropertiesDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

}  // namespace performance_manager
