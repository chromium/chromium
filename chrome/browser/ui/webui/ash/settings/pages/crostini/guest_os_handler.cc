// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/crostini/guest_os_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace ash::settings {

namespace {

base::Value::List GetSharableUsbDevices(CrosUsbDetector* detector) {
  base::Value::List usb_devices_list;
  for (const auto& device : detector->GetShareableDevices()) {
    base::Value::Dict device_info;
    device_info.Set("guid", device.guid);
    device_info.Set("label", device.label);
    if (device.shared_guest_id.has_value()) {
      base::Value::Dict guest_id;
      guest_id.Set("vm_name", device.shared_guest_id->vm_name);
      guest_id.Set("container_name", device.shared_guest_id->container_name);
      device_info.Set("guestId", std::move(guest_id));
    }
    device_info.Set("vendorId", base::StringPrintf("%04x", device.vendor_id));
    device_info.Set("productId", base::StringPrintf("%04x", device.product_id));
    device_info.Set("serialNumber", device.serial_number);
    device_info.Set("promptBeforeSharing", device.prompt_before_sharing);
    usb_devices_list.Append(std::move(device_info));
  }
  return usb_devices_list;
}

}  // namespace

GuestOsHandler::GuestOsHandler(Profile* profile) : profile_(profile) {}

GuestOsHandler::~GuestOsHandler() = default;

void GuestOsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getGuestOsSharedPathsDisplayText",
      base::BindRepeating(
          &GuestOsHandler::HandleGetGuestOsSharedPathsDisplayText,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeGuestOsSharedPath",
      base::BindRepeating(&GuestOsHandler::HandleRemoveGuestOsSharedPath,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyGuestOsSharedUsbDevicesPageReady",
      base::BindRepeating(
          &GuestOsHandler::HandleNotifyGuestOsSharedUsbDevicesPageReady,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setGuestOsUsbDeviceShared",
      base::BindRepeating(&GuestOsHandler::HandleSetGuestOsUsbDeviceShared,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GuestOsHandler::OnJavascriptAllowed() {
  if (auto* detector = CrosUsbDetector::Get()) {
    cros_usb_device_observation_.Observe(detector);
  }
}

void GuestOsHandler::OnJavascriptDisallowed() {
  cros_usb_device_observation_.Reset();
}

void GuestOsHandler::HandleGetGuestOsSharedPathsDisplayText(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  std::string callback_id = args[0].GetString();

  base::Value::List texts;
  for (const auto& path : args[1].GetList()) {
    texts.Append(file_manager::util::GetPathDisplayTextForSettings(
        profile_, path.GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void GuestOsHandler::HandleRemoveGuestOsSharedPath(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  std::string callback_id = args[0].GetString();
  std::string vm_name = args[1].GetString();
  std::string path = args[2].GetString();

  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->UnsharePath(
      vm_name, base::FilePath(path),
      /*unpersist=*/true,
      base::BindOnce(&GuestOsHandler::OnGuestOsSharedPathRemoved,
                     weak_ptr_factory_.GetWeakPtr(), callback_id, path));
}

void GuestOsHandler::OnGuestOsSharedPathRemoved(
    const std::string& callback_id,
    const std::string& path,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    LOG(ERROR) << "Error unsharing " << path << ": " << failure_reason;
  }
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(success));
}

void GuestOsHandler::HandleNotifyGuestOsSharedUsbDevicesPageReady(
    const base::Value::List& args) {
  AllowJavascript();
  OnUsbDevicesChanged();
}

void GuestOsHandler::HandleSetGuestOsUsbDeviceShared(
    const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  CrosUsbDetector* detector = CrosUsbDetector::Get();
  if (!detector) {
    return;
  }

  const auto guest_id =
      guest_os::GuestId(args[0].GetString(), args[1].GetString());
  const std::string& guid = args[2].GetString();
  bool shared = args[3].GetBool();

  if (shared) {
    detector->AttachUsbDeviceToGuest(guest_id, guid, base::DoNothing());
    return;
  }
  detector->DetachUsbDeviceFromVm(guest_id.vm_name, guid, base::DoNothing());
}

void GuestOsHandler::OnUsbDevicesChanged() {
  CrosUsbDetector* detector = CrosUsbDetector::Get();
  DCHECK(detector);  // This callback is called by the detector.
  FireWebUIListener("guest-os-shared-usb-devices-changed",
                    GetSharableUsbDevices(detector));
}

}  // namespace ash::settings
