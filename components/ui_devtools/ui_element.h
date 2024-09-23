// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/dom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_devtools {

class UIElementDelegate;

// UIElement type.
enum UIElementType { WINDOW, WIDGET, VIEW, ROOT, FRAMESINK, SURFACE };

class UI_DEVTOOLS_EXPORT UIElement {
 public:
  struct UI_DEVTOOLS_EXPORT UIProperty {
    UIProperty(std::string name, std::string value)
        : name_(name), value_(value) {}

    std::string name_;
    std::string value_;
  };
  struct UI_DEVTOOLS_EXPORT ClassProperties {
    ClassProperties(std::string name, std::vector<UIProperty> properties);
    ClassProperties(const ClassProperties& copy);
    ~ClassProperties();

    std::string class_name_;
    std::vector<UIProperty> properties_;
  };

  struct UI_DEVTOOLS_EXPORT Source {
    Source(std::string path, int line);

    std::string path_;
    int line_;
  };

  using UIElements = std::vector<raw_ptr<UIElement, VectorExperimental>>;

  UIElement(const UIElement&) = delete;
  UIElement& operator=(const UIElement&) = delete;
  virtual ~UIElement();

  // resets node ids to 0 so that they are reusable
  static void ResetNodeId();

  int node_id() const { return node_id_; }
  std::string GetTypeName() const;
  UIElement* parent() const { return parent_; }
  void set_parent(UIElement* parent) { parent_ = parent; }
  UIElementDelegate* delegate() const { return delegate_; }
  UIElementType type() const { return type_; }
  const UIElements& children() const { return children_; }
  bool is_updating() const { return is_updating_; }
  void set_is_updating(bool is_updating) { is_updating_ = is_updating; }
  int GetBaseStylesheetId() const { return base_stylesheet_id_; }
  void SetBaseStylesheetId(int id) { base_stylesheet_id_ = id; }

  // Gets/sets whether the element has sent its stylesheet header to the
  // frontend.
  bool header_sent() const { return header_sent_; }
  void set_header_sent() { header_sent_ = true; }

  using ElementCompare = bool (*)(const UIElement*, const UIElement*);

  // Inserts |child| in front of |before|. If |before| is null, it is inserted
  // at the end. Parent takes ownership of the added child.
  void AddChild(UIElement* child, UIElement* before = nullptr);

  // Inserts |child| according to a custom ordering function. |notify_delegate|
  // calls OnUIElementAdded(), which creates the subtree of UIElements at
  // |child|, and the corresponding DOM nodes.
  void AddOrderedChild(UIElement* child,
                       ElementCompare compare,
                       bool notify_delegate = true);

  // Removes and deletes all elements from |children_|.
  void ClearChildren();

  // Removes |child| out of |children_| without destroying |child|. The caller
  // is responsible for destroying |child|. |notify_delegate| calls
  // OnUIElementRemoved(), which destroys the DOM node for |child|.
  void RemoveChild(UIElement* child, bool notify_delegate = true);

  // Moves |child| to position |index| in |children_|.
  void ReorderChild(UIElement* child, int index);

  template <class T>
  int FindUIElementIdForBackendElement(T* element) const;

  // Returns properties grouped by the class they are from.
  virtual std::vector<ClassProperties> GetCustomPropertiesForMatchedStyle()
      const;

  virtual void GetBounds(gfx::Rect* bounds) const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void GetVisible(bool* visible) const = 0;
  virtual void SetVisible(bool visible) = 0;

  // Set this element's property values according to |text|.
  // |text| is the string passed in through StyleDeclarationEdit::text from
  // the frontend.
  virtual bool SetPropertiesFromString(const std::string& text);

  // If element exists, returns its associated native window and its screen
  // bounds. Otherwise, returns null and empty bounds.
  virtual std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const = 0;

  // Returns a list of interleaved keys and values of attributes to be displayed
  // on the element in the dev tools hierarchy view.
  virtual std::vector<std::string> GetAttributes() const = 0;

  template <typename BackingT, typename T>
  static BackingT* GetBackingElement(const UIElement* element) {
    return T::From(element);
  }

  // Called from PageAgent to repaint Views for Debug Bounds Rectangles
  virtual void PaintRect() const {}

  // Called in the constructor to initialize the element's sources.
  virtual void InitSources() {}

  // Get the sources for the element.
  std::vector<Source> GetSources();

  // Whether the Element Identifier matches the backing UI element.
  // This is used to locate a UIElement by Element Identifier set
  // on the browser side and different than node_id().
  virtual bool FindMatchByElementID(const ui::ElementIdentifier& identifier);

  virtual bool DispatchMouseEvent(protocol::DOM::MouseEvent* event);

  virtual bool DispatchKeyEvent(protocol::DOM::KeyEvent* event);

 protected:
  UIElement(const UIElementType type,
            UIElementDelegate* delegate,
            UIElement* parent);
  void AddSource(std::string path, int line);

 private:
  const int node_id_;
  const UIElementType type_;
  UIElements children_;
  raw_ptr<UIElement, DanglingUntriaged> parent_;
  raw_ptr<UIElementDelegate, DanglingUntriaged> delegate_;
  bool is_updating_ = false;
  int base_stylesheet_id_;
  bool header_sent_ = false;
  std::vector<Source> sources_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_
