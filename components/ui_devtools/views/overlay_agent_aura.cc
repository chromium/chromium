// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/overlay_agent_aura.h"

#include "components/ui_devtools/dom_agent.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/env.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {

OverlayAgentAura* OverlayAgentAura::overlay_agent_aura_ = nullptr;

OverlayAgentAura::OverlayAgentAura(DOMAgent* dom_agent)
    : OverlayAgentViews(dom_agent) {
  DCHECK(!overlay_agent_aura_);
  overlay_agent_aura_ = this;
}

OverlayAgentAura::~OverlayAgentAura() {
  RemovePreTargetHandler();
  overlay_agent_aura_ = nullptr;
}

void OverlayAgentAura::InstallPreTargetHandler() {
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

void OverlayAgentAura::RemovePreTargetHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

int OverlayAgentAura::FindElementIdTargetedByPoint(
    ui::LocatedEvent* event) const {
  gfx::Point p = event->root_location();
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::NativeWindow root_window = target->GetRootWindow();
  gfx::NativeWindow targeted_window = root_window->GetEventHandlerForPoint(p);
  if (!targeted_window)
    return 0;

  views::Widget* targeted_widget =
      views::Widget::GetWidgetForNativeWindow(targeted_window);
  if (!targeted_widget) {
    return dom_agent()
        ->element_root()
        ->FindUIElementIdForBackendElement<aura::Window>(targeted_window);
  }

  views::View* root_view = targeted_widget->GetRootView();
  DCHECK(root_view);

  gfx::Point point_in_targeted_window(p);
  aura::Window::ConvertPointToTarget(root_window, targeted_window,
                                     &point_in_targeted_window);
  views::View* targeted_view =
      root_view->GetEventHandlerForPoint(point_in_targeted_window);
  DCHECK(targeted_view);
  return dom_agent()
      ->element_root()
      ->FindUIElementIdForBackendElement<views::View>(targeted_view);
}

// static
std::unique_ptr<OverlayAgentViews> OverlayAgentViews::Create(
    DOMAgent* dom_agent) {
  return std::make_unique<OverlayAgentAura>(dom_agent);
}

}  // namespace ui_devtools
