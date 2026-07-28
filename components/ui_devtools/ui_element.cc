// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_element.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"

namespace ui_devtools {
namespace {

static int node_ids = 0;

}  // namespace

UIElement::UIProperty::UIProperty(std::string name, std::string value)
    : name_(name), value_(value) {}

UIElement::UIProperty::UIProperty(
    std::string name,
    std::string value,
    ui::metadata::MemberMetaDataBase* member_metadata,
    InstanceGetter instance_getter)
    : name_(name),
      value_(value),
      member_metadata_(member_metadata),
      instance_getter_(instance_getter) {}

UIElement::UIProperty::UIProperty(const UIElement::UIProperty& other) = default;
UIElement::UIProperty::UIProperty(UIElement::UIProperty&& other) = default;
UIElement::UIProperty& UIElement::UIProperty::operator=(
    const UIElement::UIProperty& other) = default;
UIElement::UIProperty& UIElement::UIProperty::operator=(
    UIElement::UIProperty&& other) = default;

UIElement::UIProperty::~UIProperty() = default;

UIElement::PropertyGroup::PropertyGroup(
    std::string group_name,
    InstanceGetter instance_getter,
    ui::metadata::ClassMetaData* class_metadata,
    std::vector<UIProperty> properties,
    base::RepeatingClosure on_changed_callback)
    : group_name_(group_name),
      instance_getter_(instance_getter),
      class_metadata_(class_metadata),
      properties_(properties),
      on_changed_callback_(on_changed_callback) {}

UIElement::PropertyGroup::PropertyGroup(
    std::string group_name,
    InstanceGetter instance_getter,
    std::vector<UIProperty> properties,
    CustomPropertySetter custom_setter,
    base::RepeatingClosure on_changed_callback)
    : group_name_(group_name),
      instance_getter_(instance_getter),
      properties_(properties),
      custom_setter_(custom_setter),
      on_changed_callback_(on_changed_callback) {}

UIElement::PropertyGroup::PropertyGroup(std::string group_name,
                                        std::vector<UIProperty> properties)
    : group_name_(group_name), properties_(properties) {}

UIElement::PropertyGroup::PropertyGroup(const UIElement::PropertyGroup& other) =
    default;
UIElement::PropertyGroup::PropertyGroup(UIElement::PropertyGroup&& other) =
    default;
UIElement::PropertyGroup& UIElement::PropertyGroup::operator=(
    const UIElement::PropertyGroup& other) = default;
UIElement::PropertyGroup& UIElement::PropertyGroup::operator=(
    UIElement::PropertyGroup&& other) = default;

UIElement::PropertyGroup::~PropertyGroup() = default;

UIElement::ClassProperties::ClassProperties(
    std::string class_name,
    std::vector<UIElement::UIProperty> properties)
    : class_name_(class_name), properties_(properties) {}

UIElement::ClassProperties::ClassProperties(
    const UIElement::ClassProperties& other) = default;

UIElement::ClassProperties::~ClassProperties() = default;

UIElement::Source::Source(std::string path, int line)
    : path_(path), line_(line) {}

// static
void UIElement::ResetNodeId() {
  node_ids = 0;
}

UIElement::~UIElement() {
  ClearChildren();
}

std::string UIElement::GetTypeName() const {
  switch (type_) {
    case UIElementType::ROOT:
      return "Root";
    case UIElementType::WINDOW:
      return "Window";
    case UIElementType::WIDGET:
      return "Widget";
    case UIElementType::VIEW:
      return "View";
    case UIElementType::FRAMESINK:
      return "FrameSink";
    case UIElementType::SURFACE:
      return "Surface";
  }
  NOTREACHED();
}

void UIElement::AddChild(UIElement* child, UIElement* before) {
  if (before) {
    auto iter = std::ranges::find(children_, before);
    CHECK(iter != children_.end());
    children_.insert(iter, child);
  } else {
    children_.push_back(child);
  }
  delegate_->OnUIElementAdded(this, child);
}

void UIElement::AddOrderedChild(UIElement* child,
                                ElementCompare compare,
                                bool notify_delegate) {
  auto iter =
      std::lower_bound(children_.begin(), children_.end(), child, compare);
  children_.insert(iter, child);
  if (notify_delegate)
    delegate_->OnUIElementAdded(this, child);
}

void UIElement::ClearChildren() {
  for (ui_devtools::UIElement* child : children_) {
    delegate_->OnUIElementRemoved(child);
    delete child;
  }
  children_.clear();
}

void UIElement::RemoveChild(UIElement* child, bool notify_delegate) {
  if (notify_delegate)
    delegate_->OnUIElementRemoved(child);
  auto iter = std::ranges::find(children_, child);
  CHECK(iter != children_.end());
  children_.erase(iter);
}

void UIElement::ReorderChild(UIElement* child, int index) {
  auto i = std::ranges::find(children_, child);
  CHECK(i != children_.end());
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), children_.size());

  // If |child| is already at the desired position, there's nothing to do.
  const auto pos = std::next(children_.begin(), index);
  if (i == pos)
    return;

  // Rotate |child| to be at the desired position.
  if (pos < i)
    std::rotate(pos, i, std::next(i));
  else
    std::rotate(i, std::next(i), std::next(pos));

  delegate()->OnUIElementReordered(child->parent(), child);
}

template <class T>
int UIElement::FindUIElementIdForBackendElement(T* element) const {
  NOTREACHED();
}

std::vector<UIElement::ClassProperties>
UIElement::GetCustomPropertiesForMatchedStyle() const {
  return {};
}

UIElement::UIElement(const UIElementType type,
                     UIElementDelegate* delegate,
                     UIElement* parent)
    : node_id_(++node_ids), type_(type), parent_(parent), delegate_(delegate) {
  delegate_->OnUIElementAdded(nullptr, this);
}

std::vector<UIElement::PropertyGroup> UIElement::GetPropertyGroups() const {
  return {};
}

bool UIElement::SetPropertiesFromString(size_t group_index,
                                        const std::string& text) {
  return false;
}

void UIElement::AddSource(std::string path, int line) {
  sources_.emplace_back(path, line);
}

std::vector<UIElement::Source> UIElement::GetSources() {
  if (sources_.empty())
    InitSources();

  return sources_;
}

bool UIElement::FindMatchByElementID(const ui::ElementIdentifier& identifier) {
  return false;
}

bool UIElement::DispatchMouseEvent(protocol::DOM::MouseEvent* event) {
  return false;
}

bool UIElement::DispatchKeyEvent(protocol::DOM::KeyEvent* event) {
  return false;
}

double UIElement::GetDeviceScaleFactor() const {
  // GetDeviceScaleFactor should only be called on window nodes.
  NOTREACHED();
}

}  // namespace ui_devtools
