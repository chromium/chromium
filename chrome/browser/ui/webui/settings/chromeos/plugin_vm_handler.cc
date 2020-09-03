// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

base::ListValue UsbDevicesToListValue(
    const std::vector<CrosUsbDeviceInfo>& shared_usbs) {
  base::ListValue usb_devices_list;
  for (const auto& device : shared_usbs) {
    base::Value device_info(base::Value::Type::DICTIONARY);
    device_info.SetStringKey("guid", device.guid);
    device_info.SetStringKey("label", device.label);
    auto it = device.vm_sharing_info.find(plugin_vm::kPluginVmName);
    bool shared = it != device.vm_sharing_info.end() && it->second.shared;
    device_info.SetBoolKey("shared", shared);
    usb_devices_list.Append(std::move(device_info));
  }
  return usb_devices_list;
}

}  // namespace

namespace settings {

PluginVmHandler::PluginVmHandler(Profile* profile) : profile_(profile) {}

PluginVmHandler::~PluginVmHandler() = default;

void PluginVmHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPluginVmSharedPathsDisplayText",
      base::BindRepeating(
          &PluginVmHandler::HandleGetPluginVmSharedPathsDisplayText,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removePluginVmSharedPath",
      base::BindRepeating(&PluginVmHandler::HandleRemovePluginVmSharedPath,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyPluginVmSharedUsbDevicesPageReady",
      base::BindRepeating(
          &PluginVmHandler::HandleNotifyPluginVmSharedUsbDevicesPageReady,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setPluginVmUsbDeviceShared",
      base::BindRepeating(&PluginVmHandler::HandleSetPluginVmUsbDeviceShared,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "wouldPermissionChangeRequireRelaunch",
      base::BindRepeating(
          &PluginVmHandler::HandleWouldPermissionChangeRequireRelaunch,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPluginVmPermission",
      base::BindRepeating(&PluginVmHandler::HandleSetPluginVmPermission,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "relaunchPluginVm",
      base::BindRepeating(&PluginVmHandler::HandleRelaunchPluginVm,
                          base::Unretained(this)));
}

void PluginVmHandler::OnJavascriptAllowed() {
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->AddUsbDeviceObserver(this);
  }
}

void PluginVmHandler::OnJavascriptDisallowed() {
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->RemoveUsbDeviceObserver(this);
  }
}

void PluginVmHandler::HandleGetPluginVmSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id = args->GetList()[0].GetString();

  base::ListValue texts;
  for (const auto& path : args->GetList()[1].GetList()) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, path.GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void PluginVmHandler::HandleRemovePluginVmSharedPath(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string vm_name = args->GetList()[0].GetString();
  std::string path = args->GetList()[1].GetString();

  guest_os::GuestOsSharePath::GetForProfile(profile_)->UnsharePath(
      vm_name, base::FilePath(path),
      /*unpersist=*/true,
      base::BindOnce(
          [](const std::string& path, bool result,
             const std::string& failure_reason) {
            if (!result) {
              LOG(ERROR) << "Error unsharing " << path << ": "
                         << failure_reason;
            }
          },
          path));
}

void PluginVmHandler::HandleNotifyPluginVmSharedUsbDevicesPageReady(
    const base::ListValue* args) {
  AllowJavascript();
  OnUsbDevicesChanged();
}

void PluginVmHandler::HandleSetPluginVmUsbDeviceShared(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  const auto& args_list = args->GetList();
  const std::string& guid = args_list[0].GetString();
  bool shared = args_list[1].GetBool();

  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector)
    return;

  if (shared) {
    detector->AttachUsbDeviceToVm(plugin_vm::kPluginVmName, guid,
                                  base::DoNothing());
    return;
  }
  detector->DetachUsbDeviceFromVm(plugin_vm::kPluginVmName, guid,
                                  base::DoNothing());
}

void PluginVmHandler::OnUsbDevicesChanged() {
  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector)
    return;

  FireWebUIListener(
      "plugin-vm-shared-usb-devices-changed",
      UsbDevicesToListValue(detector->GetDevicesSharableWithCrostini()));
}

void PluginVmHandler::HandleWouldPermissionChangeRequireRelaunch(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(3U, args->GetSize());
  std::string callback_id = args->GetList()[0].GetString();
  plugin_vm::PermissionType permission_type =
      static_cast<plugin_vm::PermissionType>(args->GetList()[1].GetInt());
  DCHECK(permission_type == plugin_vm::PermissionType::kCamera ||
         permission_type == plugin_vm::PermissionType::kMicrophone);
  plugin_vm::PluginVmManager* manager =
      plugin_vm::PluginVmManagerFactory::GetForProfile(profile_);
  bool current_value = manager->GetPermission(permission_type);
  bool proposed_value = args->GetList()[2].GetBool();
  bool requires_relaunch = proposed_value != current_value &&
                           manager->IsRelaunchNeededForNewPermissions();

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(requires_relaunch));
}

void PluginVmHandler::HandleSetPluginVmPermission(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  plugin_vm::PermissionType permission_type =
      static_cast<plugin_vm::PermissionType>(args->GetList()[0].GetInt());
  bool proposed_value = args->GetList()[1].GetBool();
  DCHECK(permission_type == plugin_vm::PermissionType::kCamera ||
         permission_type == plugin_vm::PermissionType::kMicrophone);
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)->SetPermission(
      permission_type, proposed_value);
}

void PluginVmHandler::HandleRelaunchPluginVm(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
      ->RelaunchPluginVm();
}

}  // namespace settings
}  // namespace chromeos
