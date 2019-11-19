// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_element.h"

#include <algorithm>

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"

namespace ui_devtools {
namespace {

static int node_ids = 0;

}  // namespace

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
  if (owns_children_) {
    for (auto* child : children_)
      delete child;
  }
  children_.clear();
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
  return std::string();
}

void UIElement::AddChild(UIElement* child, UIElement* before) {
  if (before) {
    auto iter = std::find(children_.begin(), children_.end(), before);
    DCHECK(iter != children_.end());
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
  children_.clear();
}

void UIElement::RemoveChild(UIElement* child, bool notify_delegate) {
  if (notify_delegate)
    delegate_->OnUIElementRemoved(child);
  auto iter = std::find(children_.begin(), children_.end(), child);
  DCHECK(iter != children_.end());
  children_.erase(iter);
}

void UIElement::ReorderChild(UIElement* child, int index) {
  auto i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
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
  return 0;
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

bool UIElement::SetPropertiesFromString(const std::string& text) {
  NOTREACHED();
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

}  // namespace ui_devtools
