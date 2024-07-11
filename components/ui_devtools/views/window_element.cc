// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/window_element.h"

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/devtools_event_util.h"
#include "components/ui_devtools/views/element_utility.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {
namespace {

int GetIndexOfChildInParent(aura::Window* window) {
  const aura::Window::Windows& siblings = window->parent()->children();
  auto it = base::ranges::find(siblings, window);
  CHECK(it != siblings.end(), base::NotFatalUntil::M130);
  return std::distance(siblings.begin(), it);
}

}  // namespace

WindowElement::WindowElement(aura::Window* window,
                             UIElementDelegate* ui_element_delegate,
                             UIElement* parent)
    : UIElementWithMetaData(UIElementType::WINDOW, ui_element_delegate, parent),
      window_(window) {
  if (window)
    window_->AddObserver(this);
}

WindowElement::~WindowElement() {
  if (window_)
    window_->RemoveObserver(this);
}

// Handles removing window_.
void WindowElement::OnWindowHierarchyChanging(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target == window_.get()) {
    parent()->RemoveChild(this);
    delete this;
  }
}

// Handles adding window_.
void WindowElement::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (window_.get() == params.new_parent &&
      params.receiver == params.new_parent) {
    AddChild(new WindowElement(params.target, delegate(), this));
  }
}

void WindowElement::OnWindowStackingChanged(aura::Window* window) {
  DCHECK_EQ(window_, window);
  parent()->ReorderChild(this, GetIndexOfChildInParent(window));
}

void WindowElement::OnWindowBoundsChanged(aura::Window* window,
                                          const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds,
                                          ui::PropertyChangeReason reason) {
  DCHECK_EQ(window_, window);
  delegate()->OnUIElementBoundsChanged(this);
}

// TODO (kylixrd@): Still need to add support for the following property.
//
//  cur_properties.emplace_back(
//      "is-activatable", wm::CanActivateWindow(window_) ? "true" : "false");

void WindowElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = window_->bounds();
}

void WindowElement::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void WindowElement::GetVisible(bool* visible) const {
  *visible = window_->IsVisible();
}

void WindowElement::SetVisible(bool visible) {
  if (visible)
    window_->Show();
  else
    window_->Hide();
}

std::vector<std::string> WindowElement::GetAttributes() const {
  return {"name", window_->GetName(), "active",
          ::wm::IsActiveWindow(window_) ? "true" : "false"};
}

std::pair<gfx::NativeWindow, gfx::Rect>
WindowElement::GetNodeWindowAndScreenBounds() const {
  return std::make_pair(static_cast<aura::Window*>(window_),
                        window_->GetBoundsInScreen());
}

// static
aura::Window* WindowElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::WINDOW, element->type());
  return static_cast<const WindowElement*>(element)->window_;
}

template <>
int UIElement::FindUIElementIdForBackendElement<aura::Window>(
    aura::Window* element) const {
  if (type_ == UIElementType::WINDOW &&
      UIElement::GetBackingElement<aura::Window, WindowElement>(this) ==
          element) {
    return node_id_;
  }
  for (ui_devtools::UIElement* child : children_) {
    int ui_element_id = child->FindUIElementIdForBackendElement(element);
    if (ui_element_id)
      return ui_element_id;
  }
  return 0;
}

void WindowElement::InitSources() {
  if (window_->layer()) {
    AddSource("ui/compositor/layer.h", 0);
  }
  AddSource("ui/aura/window.h", 0);
}

bool WindowElement::DispatchKeyEvent(protocol::DOM::KeyEvent* event) {
  ui::KeyEvent key_event = ConvertToUIKeyEvent(event);
  // Key events are processed differently based on classes. Character events are
  // routed to the text input client while key stroke events are propragated
  // through the normal event flow. The IME flow is bypassed.
  if (key_event.is_char()) {
    ui::InputMethod* input_method = window_->GetHost()->GetInputMethod();
    DCHECK(input_method);
    if (input_method->GetTextInputClient()) {
      input_method->GetTextInputClient()->InsertChar(key_event);
    } else {
      return false;
    }
  } else {
    window_->GetHost()->DispatchKeyEventPostIME(&key_event);
  }
  return true;
}

ui::metadata::ClassMetaData* WindowElement::GetClassMetaData() const {
  return window_->GetClassMetaData();
}

void* WindowElement::GetClassInstance() const {
  return window_;
}

ui::Layer* WindowElement::GetLayer() const {
  return window_->layer();
}

}  // namespace ui_devtools
