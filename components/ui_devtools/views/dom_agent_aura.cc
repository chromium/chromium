// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_aura.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace ui_devtools {

namespace {
using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;
}  // namespace

DOMAgentAura* DOMAgentAura::dom_agent_aura_ = nullptr;

DOMAgentAura::DOMAgentAura() {
  DCHECK(!dom_agent_aura_);
  dom_agent_aura_ = this;
  aura::Env::GetInstance()->AddObserver(this);
  for (aura::WindowTreeHost* window_tree_host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    OnHostInitialized(window_tree_host);
  }
}

DOMAgentAura::~DOMAgentAura() {
  for (aura::Window* window : roots_)
    window->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
  dom_agent_aura_ = nullptr;
}

void DOMAgentAura::OnHostInitialized(aura::WindowTreeHost* host) {
  roots_.push_back(host->window());
  host->window()->AddObserver(this);

  if (element_root() && !element_root()->is_updating()) {
    // The tree is already built, needs to update.
    UIElement* window_element =
        new WindowElement(host->window(), this, element_root());
    element_root()->AddChild(window_element);
  }
}

void DOMAgentAura::OnWindowDestroying(aura::Window* window) {
  std::erase(roots_, window);

  if (element_root() && !element_root()->is_updating()) {
    const auto& children = element_root()->children();
    auto iter = base::ranges::find(children, window, &WindowElement::From);
    if (iter != children.end()) {
      UIElement* child_element = *iter;
      element_root()->RemoveChild(child_element);
      delete child_element;
    }
  }
}

std::vector<UIElement*> DOMAgentAura::CreateChildrenForRoot() {
  std::vector<UIElement*> children;
  for (aura::Window* window : roots_) {
    UIElement* window_element = new WindowElement(window, this, element_root());
    children.push_back(window_element);
  }
  return children;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForWindow(
    UIElement* window_element_root) {
  DCHECK(window_element_root->type() == UIElementType::WINDOW);
  aura::Window* window =
      UIElement::GetBackingElement<aura::Window, WindowElement>(
          window_element_root);
  auto children = std::make_unique<protocol::Array<Node>>();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    UIElement* widget_element =
        new WidgetElement(widget, this, window_element_root);

    children->emplace_back(BuildTreeForRootWidget(widget_element));
    window_element_root->AddChild(widget_element);
  }
  for (aura::Window* child : window->children()) {
    UIElement* window_element =
        new WindowElement(child, this, window_element_root);

    children->emplace_back(BuildTreeForWindow(window_element));
    window_element_root->AddChild(window_element);
  }
  std::unique_ptr<Node> node =
      BuildNode("Window",
                std::make_unique<std::vector<std::string>>(
                    window_element_root->GetAttributes()),
                std::move(children), window_element_root->node_id());
  return node;
}

// static
std::unique_ptr<DOMAgentViews> DOMAgentViews::Create() {
  return std::make_unique<DOMAgentAura>();
}

}  // namespace ui_devtools
