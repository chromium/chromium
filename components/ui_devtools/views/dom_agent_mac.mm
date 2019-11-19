// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_mac.h"

#import <AppKit/AppKit.h>

#include "components/ui_devtools/views/widget_element.h"
#include "ui/views/widget/native_widget_mac.h"

namespace ui_devtools {

DOMAgentMac::DOMAgentMac() {}
DOMAgentMac::~DOMAgentMac() {}

protocol::Response DOMAgentMac::enable() {
  views::NativeWidgetMac::SetInitNativeWidgetCallback(base::BindRepeating(
      &DOMAgentMac::OnNativeWidgetAdded, base::Unretained(this)));
  return DOMAgent::enable();
}

protocol::Response DOMAgentMac::disable() {
  views::NativeWidgetMac::SetInitNativeWidgetCallback(
      base::RepeatingCallback<void(views::NativeWidgetMac*)>());
  for (views::Widget* widget : roots_)
    widget->RemoveObserver(this);
  roots_.clear();
  return DOMAgent::disable();
}

std::vector<UIElement*> DOMAgentMac::CreateChildrenForRoot() {
  if (roots_.size() == 0)
    InitializeRootsFromOpenWindows();

  std::vector<UIElement*> children;
  for (views::Widget* widget : roots_) {
    UIElement* widget_element = new WidgetElement(widget, this, element_root());
    children.push_back(widget_element);
  }
  return children;
}

void DOMAgentMac::OnWidgetDestroying(views::Widget* widget) {
  roots_.erase(std::find(roots_.begin(), roots_.end(), widget), roots_.end());
}

void DOMAgentMac::OnNativeWidgetAdded(views::NativeWidgetMac* native_widget) {
  views::Widget* widget = native_widget->GetWidget();
  DCHECK(widget);
  roots_.push_back(widget);
  UIElement* widget_element = new WidgetElement(widget, this, element_root());
  element_root()->AddChild(widget_element);
}

std::unique_ptr<protocol::DOM::Node> DOMAgentMac::BuildTreeForWindow(
    UIElement* window_element_root) {
  // Window elements aren't supported on Mac.
  NOTREACHED();
  return nullptr;
}

void DOMAgentMac::InitializeRootsFromOpenWindows() {
  for (NSWindow* window : [NSApp windows]) {
    if (views::Widget* widget =
            views::Widget::GetWidgetForNativeWindow(window)) {
      widget->AddObserver(this);
      roots_.push_back(widget);
    }
  }
}

// static
std::unique_ptr<DOMAgentViews> DOMAgentViews::Create() {
  return std::make_unique<DOMAgentMac>();
}

}  // namespace ui_devtools
