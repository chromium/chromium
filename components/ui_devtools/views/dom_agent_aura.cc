// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_aura.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {
namespace {

using ui_devtools::protocol::DOM::Node;
using ui_devtools::protocol::Array;
// TODO(mhashmi): Make ids reusable

views::Widget* GetWidgetFromWindow(gfx::NativeWindow window) {
  return views::Widget::GetWidgetForNativeView(window);
}

}  // namespace

DOMAgentAura::DOMAgentAura() {
  aura::Env::GetInstance()->AddObserver(this);
}

DOMAgentAura::~DOMAgentAura() {
  for (aura::Window* window : root_windows_)
    window->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

void DOMAgentAura::OnHostInitialized(aura::WindowTreeHost* host) {
  aura::Window* window = host->window();
  root_windows_.push_back(window);
  window->AddObserver(this);
}

void DOMAgentAura::OnWindowDestroying(aura::Window* window) {
  root_windows_.erase(
      std::remove(root_windows_.begin(), root_windows_.end(), window),
      root_windows_.end());
}

std::vector<UIElement*> DOMAgentAura::CreateChildrenForRoot() {
  std::vector<UIElement*> children;
  for (aura::Window* window : root_windows_) {
    UIElement* window_element = new WindowElement(window, this, element_root());
    children.push_back(window_element);
  }
  return children;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::WINDOW) {
    return BuildTreeForWindow(
        ui_element,
        UIElement::GetBackingElement<aura::Window, WindowElement>(ui_element));
  } else if (ui_element->type() == UIElementType::WIDGET) {
    return BuildTreeForRootWidget(
        ui_element,
        UIElement::GetBackingElement<views::Widget, WidgetElement>(ui_element));
  } else if (ui_element->type() == UIElementType::VIEW) {
    return BuildTreeForView(
        ui_element,
        UIElement::GetBackingElement<views::View, ViewElement>(ui_element));
  }
  return nullptr;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForWindow(
    UIElement* window_element_root,
    aura::Window* window) {
  std::unique_ptr<Array<Node>> children = Array<Node>::create();
  views::Widget* widget = GetWidgetFromWindow(window);
  if (widget) {
    UIElement* widget_element =
        new WidgetElement(widget, this, window_element_root);

    children->addItem(BuildTreeForRootWidget(widget_element, widget));
    window_element_root->AddChild(widget_element);
  }
  for (aura::Window* child : window->children()) {
    UIElement* window_element =
        new WindowElement(child, this, window_element_root);

    children->addItem(BuildTreeForWindow(window_element, child));
    window_element_root->AddChild(window_element);
  }
  std::unique_ptr<Node> node =
      BuildNode("Window", window_element_root->GetAttributes(),
                std::move(children), window_element_root->node_id());
  return node;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForRootWidget(
    UIElement* widget_element,
    views::Widget* widget) {
  std::unique_ptr<Array<Node>> children = Array<Node>::create();

  UIElement* view_element =
      new ViewElement(widget->GetRootView(), this, widget_element);

  children->addItem(BuildTreeForView(view_element, widget->GetRootView()));
  widget_element->AddChild(view_element);

  std::unique_ptr<Node> node =
      BuildNode("Widget", widget_element->GetAttributes(), std::move(children),
                widget_element->node_id());
  return node;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForView(UIElement* view_element,
                                                     views::View* view) {
  std::unique_ptr<Array<Node>> children = Array<Node>::create();

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

    children->addItem(BuildTreeForView(view_element_child, child));
  }
  std::unique_ptr<Node> node =
      BuildNode("View", view_element->GetAttributes(), std::move(children),
                view_element->node_id());
  return node;
}

}  // namespace ui_devtools
