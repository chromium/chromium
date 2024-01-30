// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_MENU_MENU_H_
#define COMPONENTS_DBUS_MENU_MENU_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dbus/menu/menu_property_list.h"
#include "components/dbus/properties/types.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace ui {
class MenuModel;
}

class DbusProperties;

// Implements the com.canonical.dbusmenu interface.
class COMPONENT_EXPORT(DBUS) DbusMenu {
 public:
  using InitializedCallback = base::OnceCallback<void(bool success)>;
  using MenuItemReference = std::pair<ui::MenuModel*, size_t>;

  // The exported DBus object will not be unregistered upon deletion.  It is the
  // responsibility of the caller to remove it after |this| is deleted.
  DbusMenu(dbus::ExportedObject* exported_object, InitializedCallback callback);

  DbusMenu(const DbusMenu&) = delete;
  DbusMenu& operator=(const DbusMenu&) = delete;

  ~DbusMenu();

  // Should be called when there's a new root menu.
  void SetModel(ui::MenuModel* model, bool send_signal);

  // Should be called when items are added/removed/reordered in a menu.  Prefer
  // this over SetModel().
  void MenuLayoutUpdated(ui::MenuModel* model);

  // Should be called when properties on (a group of) menu items change.  Prefer
  // this over SetModel().
  void MenuItemsPropertiesUpdated(
      const std::vector<MenuItemReference>& menu_items);

 private:
  struct MenuItem {
   public:
    MenuItem(int32_t id,
             MenuItemProperties&& properties,
             std::vector<int32_t>&& children,
             ui::MenuModel* menu,
             ui::MenuModel* containing_menu,
             size_t containing_menu_index);

    MenuItem(const MenuItem&) = delete;
    MenuItem& operator=(const MenuItem&) = delete;

    ~MenuItem();

    const int32_t id;
    MenuItemProperties properties;
    std::vector<int32_t> children;

    // The MenuModel corresponding to this MenuItem, or null if this MenuItem is
    // not a submenu.  This can happen for leaf items or an empty root item.
    const raw_ptr<ui::MenuModel, DanglingUntriaged> menu;
    // |containing_menu| will be null for the root item.  If it's null, then
    // |containing_menu_index| is meaningless.
    const raw_ptr<ui::MenuModel, DanglingUntriaged> containing_menu;
    const size_t containing_menu_index;
  };

  class ScopedMethodResponse {
   public:
    ScopedMethodResponse(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

    ~ScopedMethodResponse();

    dbus::MessageWriter& Writer();

    void EnsureResponse();

    dbus::MessageReader& reader() { return reader_; }

   private:
    raw_ptr<dbus::MethodCall> method_call_;
    dbus::ExportedObject::ResponseSender response_sender_;

    // |reader_| is always needed for all methods on this interface, so it's not
    // created lazily.
    dbus::MessageReader reader_;
    std::unique_ptr<dbus::MessageWriter> writer_;
    std::unique_ptr<dbus::Response> response_;
  };

  static dbus::ExportedObject::MethodCallCallback WrapMethodCallback(
      base::RepeatingCallback<void(DbusMenu::ScopedMethodResponse*)> callback);

  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Methods.
  void OnAboutToShow(ScopedMethodResponse* response);
  void OnAboutToShowGroup(ScopedMethodResponse* response);
  void OnEvent(ScopedMethodResponse* response);
  void OnEventGroup(ScopedMethodResponse* response);
  void OnGetGroupProperties(ScopedMethodResponse* response);
  void OnGetLayout(ScopedMethodResponse* response);
  void OnGetProperty(ScopedMethodResponse* response);

  bool AboutToShowImpl(int32_t id);

  bool EventImpl(dbus::MessageReader* reader, int32_t* id_error);

  // Get the next item ID, handling overflow and skipping over the root item
  // which must have ID 0.
  int32_t NextItemId();

  // Converts |menu| to zero or more MenuItem's and adds them to |items_|.
  // Returns a vector of IDs that index into |items_|.
  std::vector<int32_t> ConvertMenu(ui::MenuModel* menu);

  // A |depth| of -1 means infinite depth.  If |property_filter| is empty, all
  // properties will be sent.
  void WriteMenuItem(const MenuItem* item,
                     dbus::MessageWriter* writer,
                     int32_t depth,
                     const MenuPropertyList& property_filter) const;

  void WriteUpdatedProperties(dbus::MessageWriter* writer,
                              const MenuPropertyChanges& updated_props) const;

  // Recursively searches |item| and its descendants for the MenuItem
  // corresponding to |model|.
  MenuItem* FindMenuItemForModel(const ui::MenuModel* model,
                                 MenuItem* item) const;

  void DeleteItem(MenuItem* item);
  void DeleteItemChildren(MenuItem* item);

  void SendLayoutChangedSignal(int32_t id);

  raw_ptr<dbus::ExportedObject, DanglingUntriaged> menu_ = nullptr;

  base::RepeatingCallback<void(bool)> barrier_;

  std::unique_ptr<DbusProperties> properties_;

  uint32_t revision_ = 0;
  int32_t last_item_id_ = 0;
  std::map<int32_t, std::unique_ptr<MenuItem>> items_;

  base::WeakPtrFactory<DbusMenu> weak_factory_{this};
};

#endif  // COMPONENTS_DBUS_MENU_MENU_H_
