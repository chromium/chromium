// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_mac.h"

#import <AppKit/AppKit.h>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "components/ui_devtools/views/widget_element.h"
#include "ui/views/widget/native_widget_mac.h"

namespace ui_devtools {

DOMAgentMac::DOMAgentMac() = default;

DOMAgentMac::~DOMAgentMac() {
  CHECK(!IsInObserverList());
}

protocol::Response DOMAgentMac::enable() {
  init_native_widget_subscription_ =
      views::NativeWidgetMac::RegisterInitNativeWidgetCallback(
          base::BindRepeating(&DOMAgentMac::OnNativeWidgetAdded,
                              base::Unretained(this)));
  return DOMAgent::enable();
}

protocol::Response DOMAgentMac::disable() {
  init_native_widget_subscription_ = {};
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
  widget->RemoveObserver(this);
  roots_.erase(base::ranges::find(roots_, widget), roots_.end());
}

void DOMAgentMac::OnNativeWidgetAdded(views::NativeWidgetMac* native_widget) {
  views::Widget* widget = native_widget->GetWidget();
  DCHECK(widget);
  roots_.push_back(widget);
  UIElement* widget_element = new WidgetElement(widget, this, element_root());
  element_root()->AddChild(widget_element);
  widget->AddObserver(this);
}

std::unique_ptr<protocol::DOM::Node> DOMAgentMac::BuildTreeForWindow(
    UIElement* window_element_root) {
  // Window elements aren't supported on Mac.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void DOMAgentMac::InitializeRootsFromOpenWindows() {
  for (NSWindow* window in NSApp.windows) {
    if (views::Widget* widget =
            views::Widget::GetWidgetForNativeWindow(window)) {
      // When in immersive fullscreen mode, an overlay widget has two associated
      // NSWindows:
      // 1. An invisible one created by Chrome, which serves as an anchor
      //    for child widgets.
      // 2. A visible AppKit-owned NSToolbarFullScreenWindow.
      // We ensures here that a widget is only observed once.
      if (!widget->HasObserver(this)) {
        widget->AddObserver(this);
        roots_.push_back(widget);
      }
    }
  }
}

// static
std::unique_ptr<DOMAgentViews> DOMAgentViews::Create() {
  return std::make_unique<DOMAgentMac>();
}

}  // namespace ui_devtools
