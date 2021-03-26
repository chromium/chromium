// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include <algorithm>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/metadata/metadata_types.h"
#include "ui/views/widget/widget.h"

namespace ui_devtools {

namespace {

// Remove any custom editor "prefixes" from the property name. The prefixes must
// not be valid identifier characters.
void StripPrefix(std::string& property_name) {
  auto cur = property_name.cbegin();
  for (; cur < property_name.cend(); ++cur) {
    if ((*cur >= 'A' && *cur <= 'Z') || (*cur >= 'a' && *cur <= 'z') ||
        *cur == '_') {
      break;
    }
  }
  property_name.erase(property_name.cbegin(), cur);
}

ui::EventType GetMouseEventType(const std::string& type) {
  if (type == protocol::DOM::MouseEvent::TypeEnum::MousePressed)
    return ui::ET_MOUSE_PRESSED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseDragged)
    return ui::ET_MOUSE_DRAGGED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseReleased)
    return ui::ET_MOUSE_RELEASED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseMoved)
    return ui::ET_MOUSE_MOVED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseEntered)
    return ui::ET_MOUSE_ENTERED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseExited)
    return ui::ET_MOUSE_EXITED;
  if (type == protocol::DOM::MouseEvent::TypeEnum::MouseWheel)
    return ui::ET_MOUSEWHEEL;
  return ui::ET_UNKNOWN;
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
    : UIElement(UIElementType::VIEW, ui_element_delegate, parent), view_(view) {
  view_->AddObserver(this);
}

ViewElement::~ViewElement() {
  view_->RemoveObserver(this);
}

void ViewElement::OnChildViewRemoved(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = std::find_if(
      children().begin(), children().end(), [view](UIElement* child) {
        return view ==
               UIElement::GetBackingElement<views::View, ViewElement>(child);
      });
  DCHECK(iter != children().end());
  UIElement* child_element = *iter;
  RemoveChild(child_element);
  delete child_element;
}

void ViewElement::OnChildViewAdded(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  AddChild(new ViewElement(view, delegate(), this));
}

void ViewElement::OnChildViewReordered(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = std::find_if(
      children().begin(), children().end(), [view](UIElement* child) {
        return view ==
               UIElement::GetBackingElement<views::View, ViewElement>(child);
      });
  DCHECK(iter != children().end());
  UIElement* child_element = *iter;
  ReorderChild(child_element, parent->GetIndexOf(view));
}

void ViewElement::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(view_, view);
  delegate()->OnUIElementBoundsChanged(this);
}

std::vector<UIElement::ClassProperties>
ViewElement::GetCustomPropertiesForMatchedStyle() const {
  std::vector<UIElement::ClassProperties> ret;

  ui::Layer* layer = view_->layer();
  if (layer) {
    std::vector<UIElement::UIProperty> layer_properties;
    AppendLayerPropertiesMatchedStyle(layer, &layer_properties);
    ret.emplace_back("Layer", layer_properties);
  }

  std::vector<UIElement::UIProperty> class_properties;
  views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
  for (auto member = metadata->begin(); member != metadata->end(); member++) {
    auto flags = (*member)->GetPropertyFlags();
    if (!!(flags & views::metadata::PropertyFlags::kSerializable) ||
        !!(flags & views::metadata::PropertyFlags::kReadOnly)) {
      class_properties.emplace_back(
          (*member)->GetMemberNamePrefix() + (*member)->member_name(),
          base::UTF16ToUTF8((*member)->GetValueAsString(view_)));
    }

    if (member.IsLastMember()) {
      ret.emplace_back(member.GetCurrentCollectionName(), class_properties);
      class_properties.clear();
    }
  }
  return ret;
}

void ViewElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = view_->bounds();
}

void ViewElement::SetBounds(const gfx::Rect& bounds) {
  view_->SetBoundsRect(bounds);
}

void ViewElement::GetVisible(bool* visible) const {
  // Visibility information should be directly retrieved from View's metadata,
  // no need for this function any more.
  NOTREACHED();
}

void ViewElement::SetVisible(bool visible) {
  // Intentional No-op.
}

bool ViewElement::SetPropertiesFromString(const std::string& text) {
  bool property_set = false;
  std::vector<std::string> tokens = base::SplitString(
      text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() == 0UL)
    return false;

  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    std::string property_name = tokens.at(i);
    std::string property_value = base::ToLowerASCII(tokens.at(i + 1));

    // Remove any type editor "prefixes" from the property name.
    StripPrefix(property_name);

    views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
    views::metadata::MemberMetaDataBase* member =
        metadata->FindMemberData(property_name);
    if (!member) {
      DLOG(ERROR) << "UI DevTools: Can not find property " << property_name
                  << " in MetaData.";
      continue;
    }

    // Since DevTools frontend doesn't check the value, we do a sanity check
    // based on the allowed values specified in the metadata.
    auto valid_values = member->GetValidValues();
    if (!valid_values.empty() &&
        std::find(valid_values.begin(), valid_values.end(),
                  base::UTF8ToUTF16(property_value)) == valid_values.end()) {
      // Ignore the value.
      continue;
    }

    auto property_flags = member->GetPropertyFlags();
    if (!!(property_flags & views::metadata::PropertyFlags::kReadOnly))
      continue;
    DCHECK(!!(property_flags & views::metadata::PropertyFlags::kSerializable));
    member->SetValueAsString(view_, base::UTF8ToUTF16(property_value));
    property_set = true;
  }

  return property_set;
}

std::vector<std::string> ViewElement::GetAttributes() const {
  // TODO(lgrey): Change name to class after updating tests.
  return {"name", view_->GetClassName()};
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
  for (auto* child : children_) {
    int ui_element_id = child->FindUIElementIdForBackendElement(element);
    if (ui_element_id)
      return ui_element_id;
  }
  return 0;
}

void ViewElement::PaintRect() const {
  view()->SchedulePaint();
}

void ViewElement::InitSources() {
  if (view_->layer()) {
    AddSource("ui/compositor/layer.h", 0);
  }

  for (views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
       metadata != nullptr; metadata = metadata->parent_class_meta_data()) {
    // If class has Metadata properties, add their sources.
    if (!metadata->members().empty()) {
      AddSource(metadata->file(), metadata->line());
    }
  }
}

bool ViewElement::DispatchMouseEvent(protocol::DOM::MouseEvent* event) {
  ui::EventType event_type = GetMouseEventType(event->getType());
  int button_flags = GetButtonFlags(event->getButton());
  if (event_type == ui::ET_UNKNOWN)
    return false;
  gfx::Point location(event->getX(), event->getY());
  if (event_type == ui::ET_MOUSEWHEEL) {
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

}  // namespace ui_devtools
