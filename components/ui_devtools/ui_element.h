// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_UI_ELEMENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/dom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace ui::metadata {
class ClassMetaData;
class MemberMetaDataBase;
}  // namespace ui::metadata

namespace ui_devtools {

using InstanceGetter = base::RepeatingCallback<void*()>;
using CustomPropertySetter =
    base::RepeatingCallback<bool(const std::string& name,
                                 const std::string& value)>;

class UIElementDelegate;

// UIElement type.
enum UIElementType { WINDOW, WIDGET, VIEW, ROOT, FRAMESINK, SURFACE };

class UI_DEVTOOLS_EXPORT UIElement {
 public:
  // Please Note: Holding the `member_metadata` reference is safe here since it
  // represents a *class* type, not an instance type. It is allocated and cached
  // globally within the metadata system and it's lifetime persists until
  // shutdown. It will outlive the lifetime of any instances of the structs
  // below. Multiple instances of a given class will reference the same class
  // metadata. If the raw_ptr<> checks ever trigger as dangling, something very
  // bad has likely happened.
  struct UI_DEVTOOLS_EXPORT UIProperty {
    UIProperty(std::string name, std::string value);
    UIProperty(std::string name,
               std::string value,
               ui::metadata::MemberMetaDataBase* member_metadata,
               InstanceGetter instance_getter);
    UIProperty(const UIProperty& copy);
    UIProperty(UIProperty&& move);
    UIProperty& operator=(const UIProperty& copy);
    UIProperty& operator=(UIProperty&& move);
    ~UIProperty();

    std::string name_;
    std::string value_;
    raw_ptr<ui::metadata::MemberMetaDataBase> member_metadata_ = nullptr;
    InstanceGetter instance_getter_;
  };

  // PropertyGroup hosts properties of Layer, LayoutManager, Border, etc.. which
  // are sub-objects of certain UIElements. They have no independent UIElement
  // representation, so they're exposed through the PropertyGroup on the owning
  // UIElement.
  struct UI_DEVTOOLS_EXPORT PropertyGroup {
    PropertyGroup(
        std::string group_name,
        InstanceGetter instance_getter,
        ui::metadata::ClassMetaData* class_metadata,
        std::vector<UIProperty> properties,
        base::RepeatingClosure on_changed_callback = base::RepeatingClosure());
    PropertyGroup(
        std::string group_name,
        InstanceGetter instance_getter,
        std::vector<UIProperty> properties,
        CustomPropertySetter custom_setter,
        base::RepeatingClosure on_changed_callback = base::RepeatingClosure());
    PropertyGroup(std::string group_name, std::vector<UIProperty> properties);
    PropertyGroup(const PropertyGroup& copy);
    PropertyGroup(PropertyGroup&& move);
    PropertyGroup& operator=(const PropertyGroup& copy);
    PropertyGroup& operator=(PropertyGroup&& move);
    ~PropertyGroup();

    std::string group_name_;
    InstanceGetter instance_getter_;
    raw_ptr<ui::metadata::ClassMetaData> class_metadata_ = nullptr;
    std::vector<UIProperty> properties_;
    CustomPropertySetter custom_setter_;
    base::RepeatingClosure on_changed_callback_;

    void* GetInstance() const {
      return instance_getter_ ? instance_getter_.Run() : nullptr;
    }

    bool has_metadata() const {
      return class_metadata_ != nullptr && instance_getter_;
    }
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

  // Returns structured property groups for element and linked objects.
  virtual std::vector<PropertyGroup> GetPropertyGroups() const;

  // Returns properties grouped by the class they are from.
  virtual std::vector<ClassProperties> GetCustomPropertiesForMatchedStyle()
      const;

  virtual void GetBounds(gfx::Rect* bounds) const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void GetVisible(bool* visible) const = 0;
  virtual void SetVisible(bool visible) = 0;

  // Set this element's property values for the specified group. |text| is the
  // string passed in through StyleDeclarationEdit::text from the frontend.
  virtual bool SetPropertiesFromString(size_t group_index,
                                       const std::string& text);

  // If element exists, returns its associated native window and its screen
  // bounds. Otherwise, returns null and empty bounds.
  virtual std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const = 0;

  // Returns the bounds of the element in screen coordinates.
  virtual gfx::Rect GetNodeBoundsInScreen() const = 0;

  // Returns the device scale factor for the backing window associated with
  // element. Should only be called on window nodes, otherwise defaults to 1.0.
  virtual double GetDeviceScaleFactor() const;

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
