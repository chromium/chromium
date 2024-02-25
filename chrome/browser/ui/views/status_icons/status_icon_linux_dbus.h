// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/status_icons/concat_menu_model.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/linux/status_icon_linux.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class DbusMenu;
class DbusProperties;

// A status icon following the StatusNotifierItem specification.
// https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem/
class StatusIconLinuxDbus : public ui::StatusIconLinux,
                            public ui::SimpleMenuModel::Delegate,
                            public base::RefCounted<StatusIconLinuxDbus> {
 public:
  StatusIconLinuxDbus();

  StatusIconLinuxDbus(const StatusIconLinuxDbus&) = delete;
  StatusIconLinuxDbus& operator=(const StatusIconLinuxDbus&) = delete;

  // StatusIcon:
  void SetIcon(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;
  void RefreshPlatformContextMenu() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class base::RefCounted<StatusIconLinuxDbus>;

  ~StatusIconLinuxDbus() override;

  // Step 0: send the request to verify that the StatusNotifierWatcher service
  // is owned.
  void CheckStatusNotifierWatcherHasOwner();

  // Step 1: verify that the StatusNotifierWatcher service is owned.
  void OnNameHasOwnerResponse(dbus::Response* response);

  // Step 2: verify with the StatusNotifierWatcher that a StatusNotifierHost is
  // registered.
  void OnHostRegisteredResponse(dbus::Response* response);

  // Step 3: export methods for the StatusNotifierItem and the properties
  // interface.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void OnInitialized(bool success);
  void RegisterStatusNotifierItem();

  // Step 5: register the StatusNotifierItem with the StatusNotifierWatcher.
  void OnRegistered(dbus::Response* response);

  // Called when the name owner of StatusNotifierWatcher has changed, which
  // can happen when lock/unlock screen.
  void OnNameOwnerChangedReceived(const std::string& old_owner,
                                  const std::string& new_owner);

  // DBus methods.
  // Action       -> KDE behavior:
  // Left-click   -> Activate
  // Right-click  -> ContextMenu
  // Scroll       -> Scroll
  // Middle-click -> SecondaryActivate
  void OnActivate(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender);
  void OnContextMenu(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender sender);
  void OnScroll(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender sender);
  void OnSecondaryActivate(dbus::MethodCall* method_call,
                           dbus::ExportedObject::ResponseSender sender);

  void UpdateMenuImpl(ui::MenuModel* model, bool send_signal);

  void SetIconImpl(const gfx::ImageSkia& image, bool send_signals);

  void OnIconFileWritten(const base::FilePath& icon_file);

  void CleanupIconFile();

  scoped_refptr<dbus::Bus> bus_;

  int service_id_ = 0;
  raw_ptr<dbus::ObjectProxy, DanglingUntriaged> watcher_ = nullptr;
  raw_ptr<dbus::ExportedObject, DanglingUntriaged> item_ = nullptr;

  base::RepeatingCallback<void(bool)> barrier_;

  std::unique_ptr<DbusProperties> properties_;

  std::unique_ptr<DbusMenu> menu_;
  // A menu that contains the click action (if there is a click action) and a
  // separator (if there's a click action and delegate_->GetMenuModel() is
  // non-empty).
  std::unique_ptr<ui::SimpleMenuModel> click_action_menu_;
  // An empty menu for use in |concat_menu_| if delegate_->GetMenuModel() is
  // null.
  std::unique_ptr<ui::SimpleMenuModel> empty_menu_;
  // A concatenation of |click_action_menu_| and either
  // delegate_->GetMenuModel() or |empty_menu_| if the delegate's menu is null.
  // Appears after the other menus so that it gets destroyed first.
  std::unique_ptr<ConcatMenuModel> concat_menu_;
  // Used when the server doesn't support DBus menus and requests for us to use
  // our own menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  const bool should_write_icon_to_file_;
  const scoped_refptr<base::SequencedTaskRunner> icon_task_runner_;
  size_t icon_file_id_ = 0;
  base::FilePath icon_file_;

  base::WeakPtrFactory<StatusIconLinuxDbus> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
