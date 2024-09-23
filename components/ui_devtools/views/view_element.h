// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/ui_element_with_metadata.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui_devtools {

class UIElementDelegate;

class ViewElement : public views::ViewObserver, public UIElementWithMetaData {
 public:
  ViewElement(views::View* view,
              UIElementDelegate* ui_element_delegate,
              UIElement* parent);
  ViewElement(const ViewElement&) = delete;
  ViewElement& operator=(const ViewElement&) = delete;
  ~ViewElement() override;
  views::View* view() const { return view_; }

  // views::ViewObserver
  void OnChildViewRemoved(views::View* parent, views::View* view) override;
  void OnChildViewAdded(views::View* parent, views::View* view) override;
  void OnChildViewReordered(views::View* parent, views::View*) override;
  void OnViewBoundsChanged(views::View* view) override;

  // UIElement:
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;
  static views::View* From(const UIElement* element);
  void PaintRect() const override;
  bool FindMatchByElementID(const ui::ElementIdentifier& identifier) override;
  bool DispatchMouseEvent(protocol::DOM::MouseEvent* event) override;
  bool DispatchKeyEvent(protocol::DOM::KeyEvent* event) override;

 protected:
  ui::metadata::ClassMetaData* GetClassMetaData() const override;
  void* GetClassInstance() const override;
  ui::Layer* GetLayer() const override;

 private:
  // Clears children and rebuilds ViewElement subtree from scratch. Called if an
  // inconsistency is detected between the current tree and the tree of the
  // backing view.
  void RebuildTree();
  raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> observer_{this};
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_VIEW_ELEMENT_H_
