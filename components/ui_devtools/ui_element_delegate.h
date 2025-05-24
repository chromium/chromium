// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_DELEGATE_H_
#define COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_DELEGATE_H_

namespace ui_devtools {

class UIElement;

class UIElementDelegate {
 public:
  UIElementDelegate() = default;

  UIElementDelegate(const UIElementDelegate&) = delete;
  UIElementDelegate& operator=(const UIElementDelegate&) = delete;

  virtual ~UIElementDelegate() = default;

  virtual void OnUIElementAdded(UIElement* parent, UIElement* child) = 0;

  // Move |child| to different sibling index under |parent| in DOM tree.
  virtual void OnUIElementReordered(UIElement* parent, UIElement* child) = 0;

  // Remove ui_element in DOM tree.
  virtual void OnUIElementRemoved(UIElement* ui_element) = 0;

  // Update CSS agent when bounds change.
  virtual void OnUIElementBoundsChanged(UIElement* ui_element) = 0;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_DELEGATE_H_
