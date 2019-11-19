// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_REGISTRAR_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_REGISTRAR_X11_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

class GlobalMenuBarX11;

// Advertises our menu bars to com.canonical.AppMenu.Registrar.
//
// GlobalMenuBarRegistrarX11 is responsible for managing the dbus::Bus shared by
// each menu. We need a separate object to own the dbus channel and to
// register/unregister the mapping between a menu and the com.canonical.dbusmenu
// instance we are offering.
class GlobalMenuBarRegistrarX11 {
 public:
  static GlobalMenuBarRegistrarX11* GetInstance();

  void OnMenuBarCreated(GlobalMenuBarX11* menu);
  void OnMenuBarDestroyed(GlobalMenuBarX11* menu);

  dbus::Bus* bus() { return bus_.get(); }

 private:
  friend struct base::DefaultSingletonTraits<GlobalMenuBarRegistrarX11>;

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

  GlobalMenuBarRegistrarX11();
  ~GlobalMenuBarRegistrarX11();

  void InitializeMenu(GlobalMenuBarX11* menu);

  // Sends the actual message.
  void RegisterMenu(GlobalMenuBarX11* menu);

  void OnMenuInitialized(GlobalMenuBarX11* menu, bool success);

  void OnNameOwnerChanged(const std::string& service_owner);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* registrar_proxy_;
  bool service_has_owner_ = false;

  // Maps menus to flags that indicate if the menu has been successfully
  // initialized.
  std::map<GlobalMenuBarX11*, MenuState> menus_;

  base::WeakPtrFactory<GlobalMenuBarRegistrarX11> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GlobalMenuBarRegistrarX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_REGISTRAR_X11_H_
