// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_MAC_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_MAC_H_

#include "base/callback_list.h"
#include "components/ui_devtools/views/dom_agent_views.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class NativeWidgetMac;
}

namespace ui_devtools {

class DOMAgentMac : public DOMAgentViews, public views::WidgetObserver {
 public:
  DOMAgentMac();

  DOMAgentMac(const DOMAgentMac&) = delete;
  DOMAgentMac& operator=(const DOMAgentMac&) = delete;

  ~DOMAgentMac() override;

  void OnNativeWidgetAdded(views::NativeWidgetMac* native_widget);

  // DOMAgent
  std::vector<UIElement*> CreateChildrenForRoot() override;

  // DevTools protocol generated backend classes.
  protocol::Response enable() override;
  protocol::Response disable() override;

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;

  // DOMAgentViews
  std::unique_ptr<protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root) override;

 private:
  void InitializeRootsFromOpenWindows();

  std::vector<views::Widget*> roots_;

  // Called whenever a |NativeWidgetMac| is created.
  base::CallbackListSubscription init_native_widget_subscription_;
};
}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_MAC_H_
