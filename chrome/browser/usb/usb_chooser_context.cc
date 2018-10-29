// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "content/public/common/service_manager_connection.h"
#include "device/usb/mojo/device_manager_impl.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

const char kDeviceNameKey[] = "name";
const char kGuidKey[] = "ephemeral-guid";
const char kProductIdKey[] = "product-id";
const char kSerialNumberKey[] = "serial-number";
const char kVendorIdKey[] = "vendor-id";

// Reasons a permission may be closed. These are used in histograms so do not
// remove/reorder entries. Only add at the end just before
// WEBUSB_PERMISSION_REVOKED_MAX. Also remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum WebUsbPermissionRevoked {
  // Permission to access a USB device was revoked by the user.
  WEBUSB_PERMISSION_REVOKED = 0,
  // Permission to access an ephemeral USB device was revoked by the user.
  WEBUSB_PERMISSION_REVOKED_EPHEMERAL,
  // Maximum value for the enum.
  WEBUSB_PERMISSION_REVOKED_MAX
};

void RecordPermissionRevocation(WebUsbPermissionRevoked kind) {
  UMA_HISTOGRAM_ENUMERATION("WebUsb.PermissionRevoked", kind,
                            WEBUSB_PERMISSION_REVOKED_MAX);
}

bool CanStorePersistentEntry(const device::mojom::UsbDeviceInfo& device_info) {
  return !device_info.serial_number->empty();
}

base::DictionaryValue DeviceInfoToDictValue(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::DictionaryValue device_dict;
  device_dict.SetKey(kDeviceNameKey,
                     device_info.product_name
                         ? base::Value(*device_info.product_name)
                         : base::Value(""));

  if (!CanStorePersistentEntry(device_info)) {
    device_dict.SetKey(kGuidKey, base::Value(device_info.guid));
    return device_dict;
  }
  device_dict.SetKey(kVendorIdKey, base::Value(device_info.vendor_id));
  device_dict.SetKey(kProductIdKey, base::Value(device_info.product_id));
  device_dict.SetKey(kSerialNumberKey,
                     device_info.serial_number
                         ? base::Value(*device_info.serial_number)
                         : base::Value(""));
  return device_dict;
}

}  // namespace

void UsbChooserContext::Observer::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::Observer::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::Observer::OnDeviceManagerConnectionError() {}

UsbChooserContext::UsbChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         CONTENT_SETTINGS_TYPE_USB_GUARD,
                         CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()),
      client_binding_(this),
      weak_factory_(this) {
  usb_policy_allowed_devices_.reset(
      new UsbPolicyAllowedDevices(profile->GetPrefs()));
}

void UsbChooserContext::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Request UsbDeviceManagerPtr from DeviceService.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&device_manager_));
  SetUpDeviceManagerConnection();
}

void UsbChooserContext::SetUpDeviceManagerConnection() {
  DCHECK(device_manager_);
  device_manager_.set_connection_error_handler(
      base::BindOnce(&UsbChooserContext::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_binding_);
  device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  device_manager_->SetClient(std::move(client));
}

UsbChooserContext::~UsbChooserContext() {
  OnDeviceManagerConnectionError();
}

std::vector<std::unique_ptr<base::DictionaryValue>>
UsbChooserContext::GetGrantedObjects(const GURL& requesting_origin,
                                     const GURL& embedding_origin) {
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      ChooserContextBase::GetGrantedObjects(requesting_origin,
                                            embedding_origin);

  if (CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      for (const std::string& guid : it->second) {
        auto dict_it = ephemeral_dicts_.find(guid);
        DCHECK(dict_it != ephemeral_dicts_.end());
        objects.push_back(dict_it->second.CreateDeepCopy());
      }
    }
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
UsbChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetAllGrantedObjects();

  for (const auto& map_entry : ephemeral_devices_) {
    const GURL& requesting_origin = map_entry.first.first;
    const GURL& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const std::string& guid : map_entry.second) {
      auto dict_it = ephemeral_dicts_.find(guid);
      DCHECK(dict_it != ephemeral_dicts_.end());
      // ChooserContextBase::Object constructor will swap the object, so
      // a deep copy is needed here.
      auto object = dict_it->second.CreateDeepCopy();
      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          requesting_origin, embedding_origin, object.get(), "preference",
          is_incognito_));
    }
  }

  return objects;
}

void UsbChooserContext::RevokeObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::DictionaryValue& object) {
  std::string guid;
  if (object.GetString(kGuidKey, &guid)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      it->second.erase(guid);
      if (it->second.empty())
        ephemeral_devices_.erase(it);
    }
    RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED_EPHEMERAL);
  } else {
    ChooserContextBase::RevokeObjectPermission(requesting_origin,
                                               embedding_origin, object);
    RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED);
  }
}

void UsbChooserContext::GrantDevicePermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (CanStorePersistentEntry(device_info)) {
    GrantObjectPermission(requesting_origin, embedding_origin,
                          std::make_unique<base::DictionaryValue>(
                              DeviceInfoToDictValue(device_info)));
  } else {
    ephemeral_devices_[std::make_pair(requesting_origin, embedding_origin)]
        .insert(device_info.guid);
    if (!base::ContainsKey(ephemeral_dicts_, device_info.guid)) {
      ephemeral_dicts_.insert(
          std::make_pair(device_info.guid, DeviceInfoToDictValue(device_info)));
    }
  }
}

bool UsbChooserContext::HasDevicePermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (UsbBlocklist::Get().IsExcluded(device_info))
    return false;

  if (usb_policy_allowed_devices_->IsDeviceAllowed(
          requesting_origin, embedding_origin, device_info)) {
    return true;
  }

  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return false;

  auto it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (it != ephemeral_devices_.end() &&
      base::ContainsKey(it->second, device_info.guid)) {
    return true;
  }

  std::vector<std::unique_ptr<base::DictionaryValue>> device_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  for (const std::unique_ptr<base::DictionaryValue>& device_dict :
       device_list) {
    int vendor_id;
    int product_id;
    base::string16 serial_number;
    if (device_dict->GetInteger(kVendorIdKey, &vendor_id) &&
        device_info.vendor_id == vendor_id &&
        device_dict->GetInteger(kProductIdKey, &product_id) &&
        device_info.product_id == product_id &&
        device_dict->GetString(kSerialNumberKey, &serial_number) &&
        device_info.serial_number == serial_number) {
      return true;
    }
  }

  return false;
}

void UsbChooserContext::GetDevices(
    device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevices(nullptr, std::move(callback));
}

void UsbChooserContext::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request,
    device::mojom::UsbDeviceClientPtr device_client) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(guid, std::move(device_request),
                             std::move(device_client));
}

void UsbChooserContext::AddObserver(Observer* observer) {
  EnsureConnectionWithDeviceManager();
  observer_list_.AddObserver(observer);
}

void UsbChooserContext::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::WeakPtr<UsbChooserContext> UsbChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool UsbChooserContext::IsValidObject(const base::DictionaryValue& object) {
  return object.size() == 4 && object.HasKey(kDeviceNameKey) &&
         object.HasKey(kVendorIdKey) && object.HasKey(kProductIdKey) &&
         object.HasKey(kSerialNumberKey);
}

std::string UsbChooserContext::GetObjectName(
    const base::DictionaryValue& object) {
  DCHECK(IsValidObject(object));
  std::string name;
  bool found = object.GetString(kDeviceNameKey, &name);
  DCHECK(found);
  return name;
}

void UsbChooserContext::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(*device_info);
}

void UsbChooserContext::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(*device_info);

  for (auto& map_entry : ephemeral_devices_)
    map_entry.second.erase(device_info->guid);

  ephemeral_dicts_.erase(device_info->guid);
}

void UsbChooserContext::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_binding_.Close();

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceManagerConnectionError();

  ephemeral_devices_.clear();
  ephemeral_dicts_.clear();
}

void UsbChooserContext::SetDeviceManagerForTesting(
    device::mojom::UsbDeviceManagerPtr fake_device_manager) {
  DCHECK(!device_manager_);
  DCHECK(fake_device_manager);
  device_manager_ = std::move(fake_device_manager);
  SetUpDeviceManagerConnection();
}
