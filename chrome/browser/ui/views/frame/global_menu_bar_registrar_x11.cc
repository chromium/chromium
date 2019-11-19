// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/global_menu_bar_registrar_x11.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace {

const char kAppMenuRegistrarName[] = "com.canonical.AppMenu.Registrar";
const char kAppMenuRegistrarPath[] = "/com/canonical/AppMenu/Registrar";
const char kAppMenuRegistrarInterface[] = "com.canonical.AppMenu.Registrar";

}  // namespace

// static
GlobalMenuBarRegistrarX11* GlobalMenuBarRegistrarX11::GetInstance() {
  return base::Singleton<GlobalMenuBarRegistrarX11>::get();
}

void GlobalMenuBarRegistrarX11::OnMenuBarCreated(GlobalMenuBarX11* menu) {
  if (base::Contains(menus_, menu)) {
    NOTREACHED();
    return;
  }
  menus_[menu] = kUninitialized;
  if (service_has_owner_)
    InitializeMenu(menu);
}

void GlobalMenuBarRegistrarX11::OnMenuBarDestroyed(GlobalMenuBarX11* menu) {
  DCHECK(base::Contains(menus_, menu));
  if (menus_[menu] == kRegistered) {
    dbus::MethodCall method_call(kAppMenuRegistrarInterface,
                                 "UnregisterWindow");
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(menu->xid());
    registrar_proxy_->CallMethod(&method_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                 base::DoNothing());
  }
  menus_.erase(menu);
}

GlobalMenuBarRegistrarX11::GlobalMenuBarRegistrarX11() {
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SESSION;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);

  registrar_proxy_ = bus_->GetObjectProxy(
      kAppMenuRegistrarName, dbus::ObjectPath(kAppMenuRegistrarPath));

  dbus::Bus::GetServiceOwnerCallback callback =
      base::BindRepeating(&GlobalMenuBarRegistrarX11::OnNameOwnerChanged,
                          weak_ptr_factory_.GetWeakPtr());
  bus_->ListenForServiceOwnerChange(kAppMenuRegistrarName, callback);
  bus_->GetServiceOwner(kAppMenuRegistrarName, callback);
}

GlobalMenuBarRegistrarX11::~GlobalMenuBarRegistrarX11() {
  DCHECK(menus_.empty());
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
}

void GlobalMenuBarRegistrarX11::InitializeMenu(GlobalMenuBarX11* menu) {
  DCHECK(base::Contains(menus_, menu));
  DCHECK_EQ(menus_[menu], kUninitialized);
  menus_[menu] = kInitializing;
  menu->Initialize(base::BindOnce(&GlobalMenuBarRegistrarX11::OnMenuInitialized,
                                  weak_ptr_factory_.GetWeakPtr(), menu));
}

void GlobalMenuBarRegistrarX11::RegisterMenu(GlobalMenuBarX11* menu) {
  DCHECK(base::Contains(menus_, menu));
  DCHECK(menus_[menu] == kInitializeSucceeded || menus_[menu] == kRegistered);
  menus_[menu] = kRegistered;
  dbus::MethodCall method_call(kAppMenuRegistrarInterface, "RegisterWindow");
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(menu->xid());
  writer.AppendObjectPath(dbus::ObjectPath(menu->GetPath()));
  registrar_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, base::DoNothing());
}

void GlobalMenuBarRegistrarX11::OnMenuInitialized(GlobalMenuBarX11* menu,
                                                  bool success) {
  DCHECK(base::Contains(menus_, menu));
  DCHECK(menus_[menu] == kInitializing);
  menus_[menu] = success ? kInitializeSucceeded : kInitializeFailed;
  if (success && service_has_owner_)
    RegisterMenu(menu);
}

void GlobalMenuBarRegistrarX11::OnNameOwnerChanged(
    const std::string& service_owner) {
  service_has_owner_ = !service_owner.empty();

  // If the name owner changed, we need to reregister all the live menus with
  // the system.
  for (const auto& pair : menus_) {
    GlobalMenuBarX11* menu = pair.first;
    switch (pair.second) {
      case kUninitialized:
        if (service_has_owner_)
          InitializeMenu(menu);
        break;
      case kInitializing:
        // Wait for Initialize() to finish.
        break;
      case kInitializeFailed:
        // Don't try to recover.
        break;
      case kInitializeSucceeded:
        if (service_has_owner_)
          RegisterMenu(menu);
        break;
      case kRegistered:
        if (service_has_owner_)
          RegisterMenu(menu);
        else
          menus_[menu] = kInitializeSucceeded;
        break;
    }
  }
}
