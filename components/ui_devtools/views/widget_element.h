// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/ui_element_with_metadata.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace ui_devtools {

class UIElementDelegate;

class WidgetElement : public views::WidgetRemovalsObserver,
                      public views::WidgetObserver,
                      public UIElementWithMetaData {
 public:
  WidgetElement(views::Widget* widget,
                UIElementDelegate* ui_element_delegate,
                UIElement* parent);
  WidgetElement(const WidgetElement&) = delete;
  WidgetElement& operator=(const WidgetElement&) = delete;
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
  bool DispatchKeyEvent(protocol::DOM::KeyEvent* event) override;

  static views::Widget* From(const UIElement* element);

 protected:
  ui::Layer* GetLayer() const override;
  ui::metadata::ClassMetaData* GetClassMetaData() const override;
  void* GetClassInstance() const override;

 private:
  raw_ptr<views::Widget> widget_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_WIDGET_ELEMENT_H_
