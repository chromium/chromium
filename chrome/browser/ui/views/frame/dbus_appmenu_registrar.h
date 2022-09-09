// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DBUS_APPMENU_REGISTRAR_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DBUS_APPMENU_REGISTRAR_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

class DbusAppmenu;

// Advertises our menu bars to com.canonical.AppMenu.Registrar.
//
// DbusAppmenuRegistrar is responsible for managing the dbus::Bus shared by
// each menu. We need a separate object to own the dbus channel and to
// register/unregister the mapping between a menu and the com.canonical.dbusmenu
// instance we are offering.
class DbusAppmenuRegistrar {
 public:
  DbusAppmenuRegistrar(const DbusAppmenuRegistrar&) = delete;
  DbusAppmenuRegistrar& operator=(const DbusAppmenuRegistrar&) = delete;
  ~DbusAppmenuRegistrar() = delete;

  static DbusAppmenuRegistrar* GetInstance();

  void OnMenuBarCreated(DbusAppmenu* menu);
  void OnMenuBarDestroyed(DbusAppmenu* menu);

  dbus::Bus* bus() { return bus_.get(); }

 private:
  friend class base::NoDestructor<DbusAppmenuRegistrar>;

  enum MenuState {
    // Initialize() hasn't been called.
    kUninitialized,

    // Initialize() has been called and we're waiting for an async result.
    kInitializing,

    // Initialize() failed, and the window will not be registered.
    kInitializeFailed,

    // Initialize() succeeded and we will register the window once the appmenu
    // registrar has an owner.
    kInitializeSucceeded,

    // Initialize() succeeded and RegisterMenu has been sent.
    kRegistered,
  };

  DbusAppmenuRegistrar();

  void InitializeMenu(DbusAppmenu* menu);

  // Sends the actual message.
  void RegisterMenu(DbusAppmenu* menu);

  void OnMenuInitialized(DbusAppmenu* menu, bool success);

  void OnNameOwnerChanged(const std::string& service_owner);

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> registrar_proxy_;
  bool service_has_owner_ = false;

  // Maps menus to flags that indicate if the menu has been successfully
  // initialized.
  std::map<DbusAppmenu*, MenuState> menus_;

  base::WeakPtrFactory<DbusAppmenuRegistrar> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DBUS_APPMENU_REGISTRAR_H_
