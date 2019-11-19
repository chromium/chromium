// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/widget/widget.h"

namespace ui_devtools {

namespace {

// Returns true if |property_name| is type SkColor, false if not. If type
// SkColor, remove the "--" from the name.
bool GetSkColorPropertyName(std::string& property_name) {
  if (property_name.length() < 2U)
    return false;

  // Check if property starts with "--", meaning its type is SkColor.
  if (property_name[0] == '-' && property_name[1] == '-') {
    // Remove "--" from |property_name|.
    base::TrimString(property_name, "-", &property_name);
    return true;
  }
  return false;
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
    if (member.GetCurrentCollectionName() == "View" &&
        class_properties.empty()) {
      gfx::Rect bounds = view_->bounds();
      class_properties.emplace_back("x", base::NumberToString(bounds.x()));
      class_properties.emplace_back("y", base::NumberToString(bounds.y()));
      class_properties.emplace_back("width",
                                    base::NumberToString(bounds.width()));
      class_properties.emplace_back("height",
                                    base::NumberToString(bounds.height()));
      class_properties.emplace_back("is-drawn",
                                    view_->IsDrawn() ? "true" : "false");
      base::string16 description = view_->GetTooltipText(gfx::Point());
      if (!description.empty())
        class_properties.emplace_back("tooltip",
                                      base::UTF16ToUTF8(description));
    }

    // Check if type is SkColor and add "--" to property name so that DevTools
    // frontend will interpret this field as a color. Also convert SkColor value
    // to rgba string.
    if ((*member)->member_type() == "SkColor") {
      SkColor color;
      if (base::StringToUint(
              base::UTF16ToUTF8((*member)->GetValueAsString(view_)), &color))
        class_properties.emplace_back("--" + (*member)->member_name(),
                                      color_utils::SkColorToRgbaString(color));
    } else
      class_properties.emplace_back(
          (*member)->member_name(),
          base::UTF16ToUTF8((*member)->GetValueAsString(view_)));

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

    // Check if property is type SkColor.
    if (GetSkColorPropertyName(property_name)) {
      // Convert from CSS color format to SkColor.
      if (!ParseColorFromFrontend(property_value, &property_value))
        continue;
    }

    views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
    views::metadata::MemberMetaDataBase* member =
        metadata->FindMemberData(property_name);
    if (!member) {
      DLOG(ERROR) << "UI DevTools: Can not find property " << property_name
                  << " in MetaData.";
      continue;
    }

    // Since DevTools frontend doesn't check the value, we do a sanity check
    // based on its type here.
    if (member->member_type() == "bool") {
      if (property_value != "true" && property_value != "false") {
        // Ignore the value.
        continue;
      }
    }

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

}  // namespace ui_devtools
