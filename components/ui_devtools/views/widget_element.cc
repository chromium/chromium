// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/widget_element.h"

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"

namespace ui_devtools {

WidgetElement::WidgetElement(views::Widget* widget,
                             UIElementDelegate* ui_element_delegate,
                             UIElement* parent)
    : UIElement(UIElementType::WIDGET, ui_element_delegate, parent),
      widget_(widget) {
  widget_->AddRemovalsObserver(this);
  widget_->AddObserver(this);
}

WidgetElement::~WidgetElement() {
  if (widget_) {
    widget_->RemoveRemovalsObserver(this);
    widget_->RemoveObserver(this);
  }
}

void WidgetElement::OnWillRemoveView(views::Widget* widget, views::View* view) {
  if (view != widget->GetRootView())
    return;
  DCHECK_EQ(1u, children().size());
  UIElement* child = children()[0];
  RemoveChild(child);
  delete child;
}

void WidgetElement::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, widget_);
  delegate()->OnUIElementBoundsChanged(this);
}

void WidgetElement::OnWidgetDestroyed(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  delegate()->OnUIElementRemoved(this);
  widget_ = nullptr;
}

void WidgetElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = widget_->GetRestoredBounds();
}

void WidgetElement::SetBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void WidgetElement::GetVisible(bool* visible) const {
  *visible = widget_->IsVisible();
}

void WidgetElement::SetVisible(bool visible) {
  if (visible == widget_->IsVisible())
    return;
  if (visible)
    widget_->Show();
  else
    widget_->Hide();
}

std::vector<std::string> WidgetElement::GetAttributes() const {
  return {"name", widget_->GetName(), "active",
          widget_->IsActive() ? "true" : "false"};
}

std::pair<gfx::NativeWindow, gfx::Rect>
WidgetElement::GetNodeWindowAndScreenBounds() const {
  return std::make_pair(widget_->GetNativeWindow(),
                        widget_->GetWindowBoundsInScreen());
}

// static
views::Widget* WidgetElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::WIDGET, element->type());
  return static_cast<const WidgetElement*>(element)->widget_;
}

void WidgetElement::InitSources() {
  AddSource("ui/views/widget/widget.h", 0);
}

template <>
int UIElement::FindUIElementIdForBackendElement<views::Widget>(
    views::Widget* element) const {
  if (type_ == UIElementType::WIDGET &&
      UIElement::GetBackingElement<views::Widget, WidgetElement>(this) ==
          element) {
    return node_id_;
  }
  for (auto* child : children_) {
    int ui_element_id = child->FindUIElementIdForBackendElement(element);
    if (ui_element_id)
      return ui_element_id;
  }
  return 0;
}

}  // namespace ui_devtools
