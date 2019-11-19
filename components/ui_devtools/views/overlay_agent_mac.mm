// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/overlay_agent_mac.h"

#import <Cocoa/Cocoa.h>

#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"

namespace ui_devtools {

OverlayAgentMac::OverlayAgentMac(DOMAgent* dom_agent)
    : OverlayAgentViews(dom_agent) {}

OverlayAgentMac::~OverlayAgentMac() {
  if (is_pretarget_handler_)
    RemovePreTargetHandler();
}

void OverlayAgentMac::InstallPreTargetHandler() {
  DCHECK(!is_pretarget_handler_);
  is_pretarget_handler_ = true;
  for (NSWindow* window in [NSApp windows]) {
    InstallPreTargetHandlerOnWidget(
        views::Widget::GetWidgetForNativeWindow(window));
  }
}

void OverlayAgentMac::RemovePreTargetHandler() {
  DCHECK(is_pretarget_handler_);
  is_pretarget_handler_ = false;
  for (NSWindow* window in [NSApp windows]) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    RemovePreTargetHandlerOnWidget(widget);
  }
}

void OverlayAgentMac::OnElementAdded(UIElement* element) {
  if (!is_pretarget_handler_ || element->type() != UIElementType::WIDGET)
    return;
  views::Widget* widget =
      UIElement::GetBackingElement<views::Widget, WidgetElement>(element);
  InstallPreTargetHandlerOnWidget(widget);
}

void OverlayAgentMac::OnWidgetDestroying(views::Widget* widget) {
  if (!is_pretarget_handler_)
    return;
  RemovePreTargetHandlerOnWidget(widget);
}

int OverlayAgentMac::FindElementIdTargetedByPoint(
    ui::LocatedEvent* event) const {
  views::View* target = static_cast<views::View*>(event->target());
  gfx::Point p = event->root_location();
  views::View* targeted_view = target->GetEventHandlerForPoint(p);
  DCHECK(targeted_view);
  return dom_agent()
      ->element_root()
      ->FindUIElementIdForBackendElement<views::View>(targeted_view);
}

protocol::Response OverlayAgentMac::enable() {
  dom_agent()->AddObserver(this);
  return OverlayAgentViews::enable();
}

protocol::Response OverlayAgentMac::disable() {
  if (is_pretarget_handler_)
    RemovePreTargetHandler();
  hideHighlight();
  dom_agent()->RemoveObserver(this);
  return OverlayAgentViews::disable();
}
void OverlayAgentMac::InstallPreTargetHandlerOnWidget(views::Widget* widget) {
  if (!widget)
    return;
  widget->GetRootView()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
  widget->AddObserver(this);
}
void OverlayAgentMac::RemovePreTargetHandlerOnWidget(views::Widget* widget) {
  if (!widget)
    return;
  widget->GetRootView()->RemovePreTargetHandler(this);
  widget->RemoveObserver(this);
}

// static
std::unique_ptr<OverlayAgentViews> OverlayAgentViews::Create(
    DOMAgent* dom_agent) {
  return std::make_unique<OverlayAgentMac>(dom_agent);
}

}  // namespace ui_devtools
