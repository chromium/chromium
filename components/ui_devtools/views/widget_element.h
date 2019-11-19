// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace ui_devtools {

class UIElementDelegate;

class WidgetElement : public views::WidgetRemovalsObserver,
                      public views::WidgetObserver,
                      public UIElement {
 public:
  WidgetElement(views::Widget* widget,
                UIElementDelegate* ui_element_delegate,
                UIElement* parent);
  ~WidgetElement() override;
  views::Widget* widget() const { return widget_; }

  // views::WidgetRemovalsObserver:
  void OnWillRemoveView(views::Widget* widget, views::View* view) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // UIElement:
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;

  static views::Widget* From(const UIElement* element);
  void InitSources() override;

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_
