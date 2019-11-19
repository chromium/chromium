// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_devtools {

class WindowElement : public aura::WindowObserver, public UIElement {
 public:
  WindowElement(aura::Window* window,
                UIElementDelegate* ui_element_delegate,
                UIElement* parent);
  ~WindowElement() override;
  aura::Window* window() const { return window_; }

  // WindowObserver:
  void OnWindowHierarchyChanging(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // UIElement:
  std::vector<UIElement::ClassProperties> GetCustomPropertiesForMatchedStyle()
      const override;
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;

  static aura::Window* From(const UIElement* element);
  void InitSources() override;

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_
