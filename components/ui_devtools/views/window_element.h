// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/views/ui_element_with_metadata.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_devtools {

class WindowElement : public aura::WindowObserver,
                      public UIElementWithMetaData {
 public:
  WindowElement(aura::Window* window,
                UIElementDelegate* ui_element_delegate,
                UIElement* parent);
  WindowElement(const WindowElement&) = delete;
  WindowElement& operator=(const WindowElement&) = delete;
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
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;
  bool DispatchKeyEvent(protocol::DOM::KeyEvent* event) override;

  static aura::Window* From(const UIElement* element);
  void InitSources() override;

 protected:
  ui::metadata::ClassMetaData* GetClassMetaData() const override;
  void* GetClassInstance() const override;
  ui::Layer* GetLayer() const override;

 private:
  raw_ptr<aura::Window> window_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_WINDOW_ELEMENT_H_
