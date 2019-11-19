// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui_devtools {

class UIElementDelegate;

class ViewElement : public views::ViewObserver, public UIElement {
 public:
  ViewElement(views::View* view,
              UIElementDelegate* ui_element_delegate,
              UIElement* parent);
  ~ViewElement() override;
  views::View* view() const { return view_; }

  // views::ViewObserver
  void OnChildViewRemoved(views::View* parent, views::View* view) override;
  void OnChildViewAdded(views::View* parent, views::View* view) override;
  void OnChildViewReordered(views::View* parent, views::View*) override;
  void OnViewBoundsChanged(views::View* view) override;

  // UIElement:
  std::vector<UIElement::ClassProperties> GetCustomPropertiesForMatchedStyle()
      const override;
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  bool SetPropertiesFromString(const std::string& text) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;
  static views::View* From(const UIElement* element);
  void PaintRect() const override;
  void InitSources() override;

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_
