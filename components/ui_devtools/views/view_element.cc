// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include <algorithm>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/devtools_event_util.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/table_layout_view.h"
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
  auto iter = std::ranges::find(children(), view, [](UIElement* child) {
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
  if (std::ranges::contains(children(), view, [](UIElement* child) {
        return UIElement::GetBackingElement<views::View, ViewElement>(child);
      })) {
    RebuildTree();
    return;
  }
  AddChild(new ViewElement(view, delegate(), this));
}

void ViewElement::OnChildViewReordered(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = std::ranges::find(children(), view, [](UIElement* child) {
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
  return {"class", std::string(view_->GetClassName()), "name",
          view_->GetObjectName()};
}

std::pair<gfx::NativeWindow, gfx::Rect>
ViewElement::GetNodeWindowAndScreenBounds() const {
  return std::make_pair(view_->GetWidget()->GetNativeWindow(),
                        view_->GetBoundsInScreen());
}

gfx::Rect ViewElement::GetNodeBoundsInScreen() const {
  return view_->GetBoundsInScreen();
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
  return std::ranges::contains(
      views::ElementTrackerViews::GetInstance()
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

std::vector<UIElement::PropertyGroup> ViewElement::GetPropertyGroups() const {
  // 1. Common properties (Layer and metadata properties) from ancestor.
  std::vector<UIElement::PropertyGroup> groups =
      UIElementWithMetaData::GetPropertyGroups();

  // 2. LayoutManager properties (if layout manager exists and has metadata)
  // Suppress exposure of LayoutManager property group for XXXLayoutView
  // classes, since XXXLayoutView exposes layout manager properties directly on
  // the View.
  const bool is_layout_view =
      views::IsViewClass<views::BoxLayoutView>(view_) ||
      views::IsViewClass<views::FlexLayoutView>(view_) ||
      views::IsViewClass<views::TableLayoutView>(view_);

  views::LayoutManager* layout_manager = view_->GetLayoutManager();
  if (!is_layout_view && layout_manager && layout_manager->GetClassMetaData()) {
    std::vector<UIElement::UIProperty> lm_props;
    ui::metadata::ClassMetaData* lm_metadata =
        layout_manager->GetClassMetaData();

    auto instance_getter = base::BindRepeating(
        [](views::View* v) -> void* {
          return v ? v->GetLayoutManager() : nullptr;
        },
        view_.get());

    for (auto member = lm_metadata->begin(); member != lm_metadata->end();
         member++) {
      auto flags = (*member)->GetPropertyFlags();
      if (!!(flags & ui::metadata::PropertyFlags::kSerializable) ||
          !!(flags & ui::metadata::PropertyFlags::kReadOnly)) {
        lm_props.emplace_back(
            base::StrCat(
                {(*member)->GetMemberNamePrefix(), (*member)->member_name()}),
            base::UTF16ToUTF8((*member)->GetValueAsString(layout_manager)),
            *member, instance_getter);
      }

      if (member.IsLastMember()) {
        groups.emplace_back(
            base::StrCat(
                {"LayoutManager (", member.GetCurrentCollectionName(), ")"}),
            instance_getter, lm_metadata, lm_props,
            base::BindRepeating(
                [](views::View* v) {
                  if (v) {
                    v->InvalidateLayout();
                  }
                },
                view_.get()));
        lm_props.clear();
      }
    }
  }

  // 3. Border properties (if border exists - using safe base interface adapter)
  views::Border* border = view_->GetBorder();
  if (border) {
    std::vector<UIElement::UIProperty> border_props;
    gfx::Insets insets = border->GetInsets();
    std::u16string insets_str =
        ui::metadata::TypeConverter<gfx::Insets>::ToString(insets);
    border_props.emplace_back("insets", base::UTF16ToUTF8(insets_str));

    std::string color_str = border->color().ToString();
    if (!color_str.empty()) {
      border_props.emplace_back("color", color_str);
    }

    auto instance_getter = base::BindRepeating(
        [](views::View* v) -> void* { return v ? v->GetBorder() : nullptr; },
        view_.get());

    auto custom_setter = base::BindRepeating(
        [](views::View* v, const std::string& name,
           const std::string& value) -> bool {
          if (!v || !v->GetBorder()) {
            return false;
          }
          if (name == "insets") {
            auto new_insets =
                ui::metadata::TypeConverter<gfx::Insets>::FromString(
                    base::UTF8ToUTF16(value));
            if (new_insets) {
              v->SetBorder(views::CreateEmptyBorder(*new_insets));
              return true;
            }
          } else if (name == "color") {
            auto new_color = ui::metadata::SkColorConverter::FromString(
                base::UTF8ToUTF16(value));
            if (new_color) {
              v->GetBorder()->SetColor(*new_color);
              return true;
            }
          }
          return false;
        },
        view_.get());

    groups.emplace_back("Border", instance_getter, border_props, custom_setter,
                        base::BindRepeating(
                            [](views::View* v) {
                              if (v) {
                                v->SchedulePaint();
                              }
                            },
                            view_.get()));
  }

  // 4. Background properties (if background exists - using safe base interface
  // adapter)
  views::Background* background = view_->GetBackground();
  if (background) {
    std::vector<UIElement::UIProperty> bg_props;
    std::string color_str = background->color().ToString();
    if (!color_str.empty()) {
      bg_props.emplace_back("color", color_str);
    }

    auto instance_getter = base::BindRepeating(
        [](views::View* v) -> void* {
          return v ? v->GetBackground() : nullptr;
        },
        view_.get());

    auto custom_setter = base::BindRepeating(
        [](views::View* v, const std::string& name,
           const std::string& value) -> bool {
          if (!v) {
            return false;
          }
          if (name == "color") {
            auto new_color = ui::metadata::SkColorConverter::FromString(
                base::UTF8ToUTF16(value));
            if (new_color) {
              v->SetBackground(views::CreateSolidBackground(*new_color));
              return true;
            }
          }
          return false;
        },
        view_.get());

    groups.emplace_back("Background", instance_getter, bg_props, custom_setter,
                        base::BindRepeating(
                            [](views::View* v) {
                              if (v) {
                                v->SchedulePaint();
                              }
                            },
                            view_.get()));
  }

  return groups;
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
