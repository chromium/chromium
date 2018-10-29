// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/ui_devtools/devtools_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_devtools {

class UIElementDelegate;

namespace protocol {
template <typename T>
class Array;
}

// UIElement type.
enum UIElementType { WINDOW, WIDGET, VIEW, ROOT, FRAMESINK, SURFACE };

class UI_DEVTOOLS_EXPORT UIElement {
 public:
  virtual ~UIElement();
  int node_id() const { return node_id_; }
  std::string GetTypeName() const;
  UIElement* parent() const { return parent_; }
  void set_parent(UIElement* parent) { parent_ = parent; }
  UIElementDelegate* delegate() const { return delegate_; }
  UIElementType type() const { return type_; }
  const std::vector<UIElement*>& children() const { return children_; }
  bool is_updating() const { return is_updating_; }
  void set_is_updating(bool is_updating) { is_updating_ = is_updating; }

  // |child| is inserted in front of |before|. If |before| is null, it
  // is inserted at the end. Parent takes ownership of the added child.
  void AddChild(UIElement* child, UIElement* before = nullptr);

  // Remove |child| out of vector |children_| but |child| is not destroyed.
  // The caller is responsible for destroying |child|.
  void RemoveChild(UIElement* child);

  // Move |child| to position new_index in |children_|.
  void ReorderChild(UIElement* child, int new_index);

  template <class T>
  int FindUIElementIdForBackendElement(T* element) const;

  // Return a vector of pairs of properties' names and values.
  virtual std::vector<std::pair<std::string, std::string>> GetCustomProperties()
      const = 0;
  virtual void GetBounds(gfx::Rect* bounds) const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void GetVisible(bool* visible) const = 0;
  virtual void SetVisible(bool visible) = 0;

  // If element exists, return its associated native window and its bounds.
  // Otherwise, return null and empty bounds.
  virtual std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndBounds()
      const = 0;
  // Get a list of interleaved keys and values of attributes to be displayed
  // on the element in the dev tools hierarchy view.
  virtual std::unique_ptr<protocol::Array<std::string>> GetAttributes()
      const = 0;

  template <typename BackingT, typename T>
  static BackingT* GetBackingElement(const UIElement* element) {
    return T::From(element);
  }

 protected:
  UIElement(const UIElementType type,
            UIElementDelegate* delegate,
            UIElement* parent);

 private:
  const int node_id_;
  const UIElementType type_;
  std::vector<UIElement*> children_;
  UIElement* parent_;
  UIElementDelegate* delegate_;
  bool is_updating_ = false;

  DISALLOW_COPY_AND_ASSIGN(UIElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_
