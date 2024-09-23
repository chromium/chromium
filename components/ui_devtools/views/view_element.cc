// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/devtools_event_util.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ui_devtools {

namespace {

ui::EventType GetMouseEventType(const std::string& type) {
  if (type == protocol::DOM::MouseEvent::TypeEnum::MousePressed)
    return ui::EventType::kMousePressed;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseDragged)
    return ui::EventType::kMouseDragged;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseReleased)
    return ui::EventType::kMouseReleased;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseMoved)
    return ui::EventType::kMouseMoved;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseEntered)
    return ui::EventType::kMouseEntered;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseExited)
    return ui::EventType::kMouseExited;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseWheel)
    return ui::EventType::kMousewheel;
  return ui::EventType::kUnknown;
}

int GetButtonFlags(const std::string& button) {
  if (button == protocol::DOM::MouseEvent::ButtonEnum::Left)
    return ui::EF_LEFT_MOUSE_BUTTON;
  if (button == protocol::DOM::MouseEvent::ButtonEnum::Right)
    return ui::EF_RIGHT_MOUSE_BUTTON;
  if (button == protocol::DOM::MouseEvent::ButtonEnum::Middle)
    return ui::EF_MIDDLE_MOUSE_BUTTON;
  if (button == protocol::DOM::MouseEvent::ButtonEnum::Back)
    return ui::EF_BACK_MOUSE_BUTTON;
  if (button == protocol::DOM::MouseEvent::ButtonEnum::Forward)
    return ui::EF_FORWARD_MOUSE_BUTTON;
  return ui::EF_NONE;
}

int GetMouseWheelXOffset(const std::string& mouse_wheel_direction) {
  if (mouse_wheel_direction ==
      protocol::DOM::MouseEvent::WheelDirectionEnum::Left)
    return ui::MouseWheelEvent::kWheelDelta;
  if (mouse_wheel_direction ==
      protocol::DOM::MouseEvent::WheelDirectionEnum::Right)
    return -ui::MouseWheelEvent::kWheelDelta;
  return 0;
}

int GetMouseWheelYOffset(const std::string& mouse_wheel_direction) {
  if (mouse_wheel_direction ==
      protocol::DOM::MouseEvent::WheelDirectionEnum::Up)
    return ui::MouseWheelEvent::kWheelDelta;
  if (mouse_wheel_direction ==
      protocol::DOM::MouseEvent::WheelDirectionEnum::Down)
    return -ui::MouseWheelEvent::kWheelDelta;
  return 0;
}

}  // namespace

ViewElement::ViewElement(views::View* view,
                         UIElementDelegate* ui_element_delegate,
                         UIElement* parent)
    : UIElementWithMetaData(UIElementType::VIEW, ui_element_delegate, parent),
      view_(view) {
  observer_.Observe(view_.get());
}

ViewElement::~ViewElement() = default;

void ViewElement::OnChildViewRemoved(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = base::ranges::find(children(), view, [](UIElement* child) {
    return UIElement::GetBackingElement<views::View, ViewElement>(child);
  });
  if (iter == children().end()) {
    RebuildTree();
    return;
  }
  UIElement* child_element = *iter;
  RemoveChild(child_element);
  delete child_element;
}

void ViewElement::OnChildViewAdded(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  if (base::Contains(children(), view, [](UIElement* child) {
        return UIElement::GetBackingElement<views::View, ViewElement>(child);
      })) {
    RebuildTree();
    return;
  }
  AddChild(new ViewElement(view, delegate(), this));
}

void ViewElement::OnChildViewReordered(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = base::ranges::find(children(), view, [](UIElement* child) {
    return UIElement::GetBackingElement<views::View, ViewElement>(child);
  });
  if (iter == children().end() ||
      children().size() != view_->children().size()) {
    RebuildTree();
    return;
  }
  UIElement* child_element = *iter;
  ReorderChild(child_element, parent->GetIndexOf(view).value());
}

void ViewElement::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(view_, view);
  delegate()->OnUIElementBoundsChanged(this);
}

void ViewElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = view_->bounds();
}

void ViewElement::SetBounds(const gfx::Rect& bounds) {
  view_->SetBoundsRect(bounds);
}

std::vector<std::string> ViewElement::GetAttributes() const {
  // TODO(lgrey): Change name to class after updating tests.
  return {"class", view_->GetClassName(), "name", view_->GetObjectName()};
}

std::pair<gfx::NativeWindow, gfx::Rect>
ViewElement::GetNodeWindowAndScreenBounds() const {
  return std::make_pair(view_->GetWidget()->GetNativeWindow(),
                        view_->GetBoundsInScreen());
}

// static
views::View* ViewElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::VIEW, element->type());
  return static_cast<const ViewElement*>(element)->view_;
}

template <>
int UIElement::FindUIElementIdForBackendElement<views::View>(
    views::View* element) const {
  if (type_ == UIElementType::VIEW &&
      UIElement::GetBackingElement<views::View, ViewElement>(this) == element) {
    return node_id_;
  }
  for (ui_devtools::UIElement* child : children_) {
    int ui_element_id = child->FindUIElementIdForBackendElement(element);
    if (ui_element_id)
      return ui_element_id;
  }
  return 0;
}

void ViewElement::PaintRect() const {
  view()->SchedulePaint();
}

bool ViewElement::FindMatchByElementID(
    const ui::ElementIdentifier& identifier) {
  return base::Contains(views::ElementTrackerViews::GetInstance()
                            ->GetAllMatchingViewsInAnyContext(identifier),
                        view_);
}

bool ViewElement::DispatchMouseEvent(protocol::DOM::MouseEvent* event) {
  ui::EventType event_type = GetMouseEventType(event->getType());
  int button_flags = GetButtonFlags(event->getButton());
  if (event_type == ui::EventType::kUnknown) {
    return false;
  }
  gfx::Point location(event->getX(), event->getY());
  if (event_type == ui::EventType::kMousewheel) {
    int x_offset = GetMouseWheelXOffset(event->getWheelDirection());
    int y_offset = GetMouseWheelYOffset(event->getWheelDirection());
    ui::MouseWheelEvent mouse_wheel_event(
        gfx::Vector2d(x_offset, y_offset), location, location,
        ui::EventTimeForNow(), button_flags, button_flags);
    view_->OnMouseWheel(mouse_wheel_event);
  } else {
    ui::MouseEvent mouse_event(event_type, location, location,
                               ui::EventTimeForNow(), button_flags,
                               button_flags);
    view_->OnMouseEvent(&mouse_event);
  }
  return true;
}

bool ViewElement::DispatchKeyEvent(protocol::DOM::KeyEvent* event) {
  ui::KeyEvent key_event = ConvertToUIKeyEvent(event);
  // Key events are processed differently based on classes. Character events are
  // routed to the text input client while key stroke events are propragated
  // through the normal event flow. The IME flow is bypassed.
  if (key_event.is_char()) {
    // Since the IME flow is bypassed, we need to manually add ui components
    // we want to receive character events here.
    if (views::IsViewClass<views::Textfield>(view_)) {
      static_cast<views::Textfield*>(view_)->InsertChar(key_event);
    } else {
      return false;
    }
  } else {
    view_->OnKeyEvent(&key_event);
  }
  return true;
}

ui::metadata::ClassMetaData* ViewElement::GetClassMetaData() const {
  return view_->GetClassMetaData();
}

void* ViewElement::GetClassInstance() const {
  return view_;
}

ui::Layer* ViewElement::GetLayer() const {
  return view_->layer();
}

void ViewElement::RebuildTree() {
  ClearChildren();
  for (views::View* child : view_->children()) {
    AddChild(new ViewElement(child, delegate(), this));
  }
}

}  // namespace ui_devtools
