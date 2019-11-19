// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_

#include "components/ui_devtools/views/dom_agent_views.h"

#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui_devtools {

class DOMAgentAura : public DOMAgentViews,
                     public aura::EnvObserver,
                     public aura::WindowObserver {
 public:
  DOMAgentAura();

  ~DOMAgentAura() override;
  static DOMAgentAura* GetInstance() { return dom_agent_aura_; }

  // DOMAgent
  std::vector<UIElement*> CreateChildrenForRoot() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  std::unique_ptr<protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root) override;

 private:
  static DOMAgentAura* dom_agent_aura_;

  std::vector<aura::Window*> roots_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgentAura);
};
}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_
