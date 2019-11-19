// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_views.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ui_devtools {
namespace {

using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;

}  // namespace

DOMAgentViews::DOMAgentViews() {}
DOMAgentViews::~DOMAgentViews() {}

std::unique_ptr<Node> DOMAgentViews::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::WINDOW) {
    return BuildTreeForWindow(ui_element);
  } else if (ui_element->type() == UIElementType::WIDGET) {
    return BuildTreeForRootWidget(ui_element);
  } else if (ui_element->type() == UIElementType::VIEW) {
    return BuildTreeForView(ui_element);
  }
  return nullptr;
}

std::unique_ptr<Node> DOMAgentViews::BuildTreeForRootWidget(
    UIElement* widget_element) {
  DCHECK(widget_element->type() == UIElementType::WIDGET);
  views::Widget* widget =
      UIElement::GetBackingElement<views::Widget, WidgetElement>(
          widget_element);

  auto children = std::make_unique<protocol::Array<Node>>();
  ViewElement* view_element =
      new ViewElement(widget->GetRootView(), this, widget_element);
  children->emplace_back(BuildTreeForView(view_element));
  widget_element->AddChild(view_element);

  std::unique_ptr<Node> node =
      BuildNode("Widget",
                std::make_unique<std::vector<std::string>>(
                    widget_element->GetAttributes()),
                std::move(children), widget_element->node_id());
  return node;
}

std::unique_ptr<Node> DOMAgentViews::BuildTreeForView(UIElement* view_element) {
  DCHECK(view_element->type() == UIElementType::VIEW);
  views::View* view =
      UIElement::GetBackingElement<views::View, ViewElement>(view_element);
  auto children = std::make_unique<protocol::Array<Node>>();

  for (auto* child : view->GetChildrenInZOrder()) {
    // When building the subtree, a particular view could be visited multiple
    // times because for each view of the subtree, we would call
    // BuildTreeForView(..) on that view which causes the subtree with that view
    // as root being visited again.  Here we check if we already constructed the
    // ViewElement and skip true.
    UIElement* view_element_child = nullptr;
    auto id =
        view_element->FindUIElementIdForBackendElement<views::View>(child);
    if (id > 0) {
      view_element_child = GetElementFromNodeId(id);
    } else {
      view_element_child = new ViewElement(child, this, view_element);
      view_element->AddChild(view_element_child);
    }

    children->emplace_back(BuildTreeForView(view_element_child));
  }
  return BuildNode(
      "View",
      std::make_unique<std::vector<std::string>>(view_element->GetAttributes()),
      std::move(children), view_element->node_id());
}

}  // namespace ui_devtools
