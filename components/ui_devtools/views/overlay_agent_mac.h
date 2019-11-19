// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_MAC_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_MAC_H_

#include "components/ui_devtools/views/overlay_agent_views.h"

#include "components/ui_devtools/dom_agent.h"
#include "ui/views/widget/widget_observer.h"

namespace ui_devtools {

class OverlayAgentMac : public OverlayAgentViews,
                        public DOMAgentObserver,
                        public views::WidgetObserver {
 public:
  OverlayAgentMac(DOMAgent* dom_agent);
  ~OverlayAgentMac() override;
  int FindElementIdTargetedByPoint(ui::LocatedEvent* event) const override;

  // DevTools protocol generated backend classes.
  protocol::Response enable() override;
  protocol::Response disable() override;

  // DOMAgentObserver
  void OnElementAdded(UIElement* ui_element) override;

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // OverlayAgentViews
  void InstallPreTargetHandler() override;
  void RemovePreTargetHandler() override;

  void InstallPreTargetHandlerOnWidget(views::Widget* widget);
  void RemovePreTargetHandlerOnWidget(views::Widget* widget);

  bool is_pretarget_handler_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverlayAgentMac);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_MAC_H_
