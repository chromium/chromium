// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/cpp/usb/usb_ids.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

constexpr char kDeviceNameKey[] = "name";
constexpr char kGuidKey[] = "ephemeral-guid";
constexpr char kProductIdKey[] = "product-id";
constexpr char kSerialNumberKey[] = "serial-number";
constexpr char kVendorIdKey[] = "vendor-id";
constexpr int kDeviceIdWildcard = -1;
constexpr int kUsbClassMassStorage = 0x08;

bool CanStorePersistentEntry(const device::mojom::UsbDeviceInfo& device_info) {
  return device_info.serial_number && !device_info.serial_number->empty();
}

std::pair<int, int> GetDeviceIds(const base::Value::Dict& object) {
  DCHECK(object.FindInt(kVendorIdKey));
  int vendor_id = *object.FindInt(kVendorIdKey);

  DCHECK(object.FindInt(kProductIdKey));
  int product_id = *object.FindInt(kProductIdKey);

  return std::make_pair(vendor_id, product_id);
}

std::u16string GetDeviceNameFromIds(int vendor_id, int product_id) {
#if !BUILDFLAG(IS_ANDROID)
  const char* product_name =
      device::UsbIds::GetProductName(vendor_id, product_id);
  if (product_name)
    return base::UTF8ToUTF16(product_name);

  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    if (product_id == kDeviceIdWildcard) {
      return l10n_util::GetStringFUTF16(
          IDS_USB_POLICY_DEVICE_DESCRIPTION_FOR_VENDOR_NAME,
          base::UTF8ToUTF16(vendor_name));
    }

    return l10n_util::GetStringFUTF16(
        IDS_USB_POLICY_DEVICE_DESCRIPTION_FOR_PRODUCT_ID_AND_VENDOR_NAME,
        base::ASCIIToUTF16(base::StringPrintf("0x%04X", product_id)),
        base::UTF8ToUTF16(vendor_name));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (product_id == kDeviceIdWildcard) {
    if (vendor_id == kDeviceIdWildcard)
      return l10n_util::GetStringUTF16(
          IDS_USB_POLICY_DEVICE_DESCRIPTION_FOR_ANY_VENDOR);

    return l10n_util::GetStringFUTF16(
        IDS_USB_POLICY_DEVICE_DESCRIPTION_FOR_VENDOR_ID,
        base::ASCIIToUTF16(base::StringPrintf("0x%04X", vendor_id)));
  }

  return l10n_util::GetStringFUTF16(
      IDS_USB_POLICY_DEVICE_DESCRIPTION_FOR_PRODUCT_ID_AND_VENDOR_ID,
      base::ASCIIToUTF16(base::StringPrintf("0x%04X", product_id)),
      base::ASCIIToUTF16(base::StringPrintf("0x%04X", vendor_id)));
}

base::Value::Dict DeviceIdsToValue(int vendor_id, int product_id) {
  base::Value::Dict device_value;
  std::u16string device_name = GetDeviceNameFromIds(vendor_id, product_id);

  device_value.Set(kDeviceNameKey, device_name);
  device_value.Set(kVendorIdKey, vendor_id);
  device_value.Set(kProductIdKey, product_id);
  device_value.Set(kSerialNumberKey, std::string());

  return device_value;
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsDetachable(int vid, int pid) {
  // TOOD(huangs): Figure out how to do the following in Lacros, which does not
  // have access to ash::CrosSettings (https://crbug.com/1219329).
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::Value::List* policy_list;
  if (ash::CrosSettings::Get()->GetList(ash::kUsbDetachableAllowlist,
                                        &policy_list)) {
    for (const auto& entry : *policy_list) {
      if (entry.GetDict().FindInt(ash::kUsbDetachableAllowlistKeyVid) == vid &&
          entry.GetDict().FindInt(ash::kUsbDetachableAllowlistKeyPid) == pid) {
        return true;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const crosapi::mojom::DeviceSettings* device_settings =
      chromeos::BrowserParamsProxy::Get()->DeviceSettings().get();
  if (device_settings && device_settings->usb_detachable_allow_list) {
    for (const auto& entry :
         device_settings->usb_detachable_allow_list->usb_device_ids) {
      if (entry->has_vendor_id && entry->vendor_id == vid &&
          entry->has_product_id && entry->product_id == pid) {
        return true;
      }
    }
  }
#endif
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsMassStorageInterface(const device::mojom::UsbInterfaceInfo& interface) {
  for (auto& alternate : interface.alternates) {
    if (alternate->class_code == kUsbClassMassStorage)
      return true;
  }
  return false;
}

bool ShouldExposeDevice(const device::mojom::UsbDeviceInfo& device_info) {
#if BUILDFLAG(IS_CHROMEOS)
  if (IsDetachable(device_info.vendor_id, device_info.product_id))
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // blink::USBDevice::claimInterface() disallows claiming mass storage
  // interfaces, but explicitly prevent access in the browser process as
  // ChromeOS would allow these interfaces to be claimed.
  for (auto& configuration : device_info.configurations) {
    if (configuration->interfaces.size() == 0) {
        return true;
    }
    for (auto& interface : configuration->interfaces) {
      if (!IsMassStorageInterface(*interface))
        return true;
    }
  }
  return false;
}

}  // namespace

void UsbChooserContext::DeviceObserver::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::DeviceObserver::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::DeviceObserver::OnDeviceManagerConnectionError() {}

UsbChooserContext::UsbChooserContext(Profile* profile)
    : ObjectPermissionContextBase(
          ContentSettingsType::USB_GUARD,
          ContentSettingsType::USB_CHOOSER_DATA,
          HostContentSettingsMapFactory::GetForProfile(profile)),
      is_incognito_(profile->IsOffTheRecord()) {
  usb_policy_allowed_devices_ =
      std::make_unique<UsbPolicyAllowedDevices>(profile->GetPrefs());
}

// static
base::Value::Dict UsbChooserContext::DeviceInfoToValue(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::Value::Dict device_value;
  device_value.Set(kDeviceNameKey, device_info.product_name
                                       ? *device_info.product_name
                                       : std::u16string_view());
  device_value.Set(kVendorIdKey, device_info.vendor_id);
  device_value.Set(kProductIdKey, device_info.product_id);

  // CanStorePersistentEntry checks if |device_info.serial_number| is not empty.
  if (CanStorePersistentEntry(device_info)) {
    device_value.Set(kSerialNumberKey, *device_info.serial_number);
  } else {
    device_value.Set(kGuidKey, device_info.guid);
  }

  return device_value;
}

void UsbChooserContext::InitDeviceList(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    if (ShouldExposeDevice(*device_info)) {
      devices_.insert(
          std::make_pair(device_info->guid, std::move(device_info)));
    }
  }
  is_initialized_ = true;

  while (!pending_get_devices_requests_.empty()) {
    std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
    for (const auto& entry : devices_) {
      device_list.push_back(entry.second->Clone());
    }
    std::move(pending_get_devices_requests_.front())
        .Run(std::move(device_list));
    pending_get_devices_requests_.pop();
  }
}

void UsbChooserContext::Shutdown() {
  FlushScheduledSaveSettingsCalls();
  permissions::ObjectPermissionContextBase::Shutdown();
}

void UsbChooserContext::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Receive mojo::Remote<UsbDeviceManager> from DeviceService.
  content::GetDeviceService().BindUsbDeviceManager(
      device_manager_.BindNewPipeAndPassReceiver());

  SetUpDeviceManagerConnection();
}

void UsbChooserContext::SetUpDeviceManagerConnection() {
  DCHECK(device_manager_);
  device_manager_.set_disconnect_handler(
      base::BindOnce(&UsbChooserContext::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  device_manager_->EnumerateDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&UsbChooserContext::InitDeviceList,
                     weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_ANDROID)
void UsbChooserContext::OnDeviceInfoRefreshed(
    device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback,
    device::mojom::UsbDeviceInfoPtr device_info) {
  if (!device_info) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto it = devices_.find(device_info->guid);
  if (it == devices_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  it->second = std::move(device_info);
  std::move(callback).Run(it->second->Clone());
}
#endif

UsbChooserContext::~UsbChooserContext() {
  OnDeviceManagerConnectionError();
  for (auto& observer : device_observer_list_) {
    observer.OnBrowserContextShutdown();
    DCHECK(!device_observer_list_.HasObserver(&observer));
  }
  DCHECK(permission_observer_list_.empty());
}

std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
UsbChooserContext::GetGrantedObjects(const url::Origin& origin) {
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetGrantedObjects(origin);

  if (CanRequestObjectPermission(origin)) {
    auto it = ephemeral_devices_.find(origin);
    if (it != ephemeral_devices_.end()) {
      for (const std::string& guid : it->second) {
        // |devices_| should be initialized when |ephemeral_devices_| is filled.
        // Because |ephemeral_devices_| is filled by GrantDevicePermission()
        // which is called in UsbChooserController::Select(), this method will
        // always be called after device initialization in UsbChooserController
        // which always returns after the device list initialization in this
        // class.
        DCHECK(base::Contains(devices_, guid));
        objects.push_back(std::make_unique<Object>(
            origin, DeviceInfoToValue(*devices_[guid]),
            content_settings::SettingSource::kUser, is_incognito_));
      }
    }
  }

  // Iterate through the user granted objects and create a mapping of device IDs
  // to device object if the object is also allowed by policy. Any objects that
  // have been granted by policy are removed from |objects| to avoid duplicate
  // permissions from being displayed.
  // TODO(crbug.com/40611788): This logic is very similar to the logic for
  // GetAllGrantedObjects(), so it could potentially be centralized.
  std::map<std::pair<int, int>, base::Value::Dict> device_ids_to_object_map;
  for (auto it = objects.begin(); it != objects.end();) {
    base::Value::Dict& object = (*it)->value;
    auto device_ids = GetDeviceIds(object);

    if (usb_policy_allowed_devices_->IsDeviceAllowed(origin, device_ids)) {
      device_ids_to_object_map[device_ids] = std::move(object);
      it = objects.erase(it);
    } else {
      ++it;
    }
  }

  for (const auto& allowed_devices_entry : usb_policy_allowed_devices_->map()) {
    // The map key is a tuple of (vendor_id, product_id).
    const int vendor_id = allowed_devices_entry.first.first;
    const int product_id = allowed_devices_entry.first.second;

    for (const auto& url : allowed_devices_entry.second) {
      // Skip entries that do not match the |origin|.
      if (url != origin)
        continue;

      // If there is an entry for the device in |device_ids_to_object_map|, use
      // that object to represent the device. Otherwise, attempt to figure out
      // the name of the device from the |vendor_id| and |product_id|.
      base::Value::Dict object;
      auto it =
          device_ids_to_object_map.find(std::make_pair(vendor_id, product_id));
      if (it != device_ids_to_object_map.end()) {
        object = std::move(it->second);
      } else {
        object = DeviceIdsToValue(vendor_id, product_id);
      }

      objects.push_back(std::make_unique<Object>(
          url, std::move(object), content_settings::SettingSource::kPolicy,
          is_incognito_));
    }
  }

  return objects;
}

std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
UsbChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetAllGrantedObjects();

  for (const auto& map_entry : ephemeral_devices_) {
    const url::Origin& origin = map_entry.first;

    if (!CanRequestObjectPermission(origin))
      continue;

    for (const std::string& guid : map_entry.second) {
      DCHECK(base::Contains(devices_, guid));
      objects.push_back(std::make_unique<Object>(
          origin, DeviceInfoToValue(*devices_[guid]),
          content_settings::SettingSource::kUser, is_incognito_));
    }
  }

  // Iterate through the user granted objects to create a mapping of device IDs
  // to device object for the policy granted objects to use, and remove
  // objects that have already been granted permission by the policy.
  // TODO(crbug.com/40611788): This logic is very similar to the logic for
  // GetGrantedObjects(), so it could potentially be centralized.
  std::map<std::pair<int, int>, base::Value::Dict> device_ids_to_object_map;
  for (auto it = objects.begin(); it != objects.end();) {
    Object& object = **it;
    auto device_ids = GetDeviceIds(object.value);
    auto origin = url::Origin::Create(object.origin);

    if (usb_policy_allowed_devices_->IsDeviceAllowed(origin, device_ids)) {
      device_ids_to_object_map[device_ids] = std::move(object.value);
      it = objects.erase(it);
    } else {
      ++it;
    }
  }

  for (const auto& allowed_devices_entry : usb_policy_allowed_devices_->map()) {
    // The map key is a tuple of (vendor_id, product_id).
    const int vendor_id = allowed_devices_entry.first.first;
    const int product_id = allowed_devices_entry.first.second;

    for (const auto& url : allowed_devices_entry.second) {
      // If there is an entry for the device in |device_ids_to_object_map|, use
      // that object to represent the device. Otherwise, attempt to figure out
      // the name of the device from the |vendor_id| and |product_id|.
      base::Value::Dict object;
      auto it =
          device_ids_to_object_map.find(std::make_pair(vendor_id, product_id));
      if (it != device_ids_to_object_map.end()) {
        object = it->second.Clone();
      } else {
        object = DeviceIdsToValue(vendor_id, product_id);
      }

      objects.push_back(std::make_unique<Object>(
          url, std::move(object), content_settings::SettingSource::kPolicy,
          is_incognito_));
    }
  }

  return objects;
}

void UsbChooserContext::RevokeObjectPermission(
    const url::Origin& origin,
    const base::Value::Dict& object) {
  RevokeObjectPermissionInternal(origin, object, /*revoked_by_website=*/false);
}

void UsbChooserContext::RevokeDevicePermissionWebInitiated(
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device) {
  DCHECK(base::Contains(devices_, device.guid));
  RevokeObjectPermissionInternal(origin, DeviceInfoToValue(device),
                                 /*revoked_by_website=*/true);
}

void UsbChooserContext::RevokeObjectPermissionInternal(
    const url::Origin& origin,
    const base::Value::Dict& object,
    bool revoked_by_website = false) {
  const std::string* guid = object.FindString(kGuidKey);

  if (!guid) {
    ObjectPermissionContextBase::RevokeObjectPermission(origin, object);
    RecordWebUsbPermissionRevocation(revoked_by_website
                                         ? WEBUSB_PERMISSION_REVOKED_BY_WEBSITE
                                         : WEBUSB_PERMISSION_REVOKED_BY_USER);
    return;
  }

  auto it = ephemeral_devices_.find(origin);
  if (it != ephemeral_devices_.end()) {
    it->second.erase(*guid);
    if (it->second.empty())
      ephemeral_devices_.erase(it);
    NotifyPermissionRevoked(origin);
  }

  RecordWebUsbPermissionRevocation(
      revoked_by_website ? WEBUSB_PERMISSION_REVOKED_EPHEMERAL_BY_WEBSITE
                         : WEBUSB_PERMISSION_REVOKED_EPHEMERAL_BY_USER);
}

std::string UsbChooserContext::GetKeyForObject(
    const base::Value::Dict& object) {
  if (!IsValidObject(object))
    return std::string();
  return base::JoinString(
      {base::NumberToString(*(object.FindInt(kVendorIdKey))),
       base::NumberToString(*(object.FindInt(kProductIdKey))),
       *(object.FindString(kSerialNumberKey))},
      "|");
}

bool UsbChooserContext::IsValidObject(const base::Value::Dict& object) {
  return object.size() == 4 && object.FindString(kDeviceNameKey) &&
         object.FindInt(kVendorIdKey) && object.FindInt(kProductIdKey) &&
         (object.FindString(kSerialNumberKey) || object.FindString(kGuidKey));
}

std::u16string UsbChooserContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  const std::string* name = object.FindString(kDeviceNameKey);
  DCHECK(name);
  if (!name->empty())
    return base::UTF8ToUTF16(*name);

  std::optional<int> vendor_id = object.FindInt(kVendorIdKey);
  std::optional<int> product_id = object.FindInt(kProductIdKey);
  DCHECK(vendor_id && product_id);
  return GetDeviceNameFromIds(*vendor_id, *product_id);
}

void UsbChooserContext::GrantDevicePermission(
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (CanStorePersistentEntry(device_info)) {
    GrantObjectPermission(origin, DeviceInfoToValue(device_info));
  } else {
    ephemeral_devices_[origin].insert(device_info.guid);
    NotifyPermissionChanged();
  }
}

bool UsbChooserContext::HasDevicePermission(
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device_info) {

  if (usb_policy_allowed_devices_->IsDeviceAllowed(origin, device_info)) {
    return true;
  }

  if (!CanRequestObjectPermission(origin))
    return false;

  auto it = ephemeral_devices_.find(origin);
  if (it != ephemeral_devices_.end() &&
      base::Contains(it->second, device_info.guid)) {
    return true;
  }

  std::vector<std::unique_ptr<Object>> object_list = GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    const base::Value::Dict& device = object->value;
    DCHECK(IsValidObject(device));

    const int vendor_id = *device.FindInt(kVendorIdKey);
    const int product_id = *device.FindInt(kProductIdKey);
    const std::string* serial_number = device.FindString(kSerialNumberKey);
    if (device_info.vendor_id == vendor_id &&
        device_info.product_id == product_id && serial_number &&
        device_info.serial_number == base::UTF8ToUTF16(*serial_number)) {
      return true;
    }
  }

  return false;
}

void UsbChooserContext::GetDevices(
    device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
  if (!is_initialized_) {
    EnsureConnectionWithDeviceManager();
    pending_get_devices_requests_.push(std::move(callback));
    return;
  }

  std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
  for (const auto& pair : devices_)
    device_list.push_back(pair.second->Clone());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_list)));
}

void UsbChooserContext::GetDevice(
    const std::string& guid,
    base::span<const uint8_t> blocked_interface_classes,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(
      guid,
      std::vector<uint8_t>(blocked_interface_classes.begin(),
                           blocked_interface_classes.end()),
      std::move(device_receiver), std::move(device_client));
}

const device::mojom::UsbDeviceInfo* UsbChooserContext::GetDeviceInfo(
    const std::string& guid) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  return it == devices_.end() ? nullptr : it->second.get();
}

#if BUILDFLAG(IS_ANDROID)
void UsbChooserContext::RefreshDeviceInfo(
    const std::string& guid,
    device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback) {
  EnsureConnectionWithDeviceManager();
  device_manager_->RefreshDeviceInfo(
      guid, base::BindOnce(&UsbChooserContext::OnDeviceInfoRefreshed,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}
#endif

void UsbChooserContext::AddObserver(DeviceObserver* observer) {
  EnsureConnectionWithDeviceManager();
  device_observer_list_.AddObserver(observer);
}

void UsbChooserContext::RemoveObserver(DeviceObserver* observer) {
  device_observer_list_.RemoveObserver(observer);
}

base::WeakPtr<UsbChooserContext> UsbChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void UsbChooserContext::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
  // Update the device list.
  DCHECK(!base::Contains(devices_, device_info->guid));
  if (!ShouldExposeDevice(*device_info))
    return;
  devices_.insert(std::make_pair(device_info->guid, device_info->Clone()));

  // Notify all observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceAdded(*device_info);
}

void UsbChooserContext::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  if (!ShouldExposeDevice(*device_info)) {
    DCHECK(!base::Contains(devices_, device_info->guid));
    return;
  }

  // Update the device list.
  DCHECK(base::Contains(devices_, device_info->guid));
  devices_.erase(device_info->guid);

  // Notify all device observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceRemoved(*device_info);

  // If the device was persistent, return. Otherwise, notify all permission
  // observers that its permissions were revoked.
  if (device_info->serial_number &&
      !device_info->serial_number.value().empty()) {
    return;
  }

  std::vector<url::Origin> revoked_urls;
  for (auto& map_entry : ephemeral_devices_) {
    if (map_entry.second.erase(device_info->guid) > 0)
      revoked_urls.push_back(map_entry.first);
  }

  for (auto& observer : permission_observer_list_) {
    observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                       data_content_settings_type_);
    for (auto& url : revoked_urls) {
      observer.OnPermissionRevoked(url);
    }
  }
}

void UsbChooserContext::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_receiver_.reset();
  devices_.clear();
  is_initialized_ = false;

  // Store the revoked URLs to notify observers of the revoked permissions.
  std::vector<url::Origin> revoked_origins;
  for (auto& map_entry : ephemeral_devices_)
    revoked_origins.push_back(map_entry.first);
  ephemeral_devices_.clear();

  // Notify all device observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceManagerConnectionError();

  // Notify all permission observers.
  for (auto& observer : permission_observer_list_) {
    observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                       data_content_settings_type_);
    for (auto& origin : revoked_origins) {
      observer.OnPermissionRevoked(origin);
    }
  }
}

void UsbChooserContext::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager) {
  // `device_manager_` can be bound in some test scenarios, in that case, just
  // reset the connection.
  if (device_manager_) {
    device_manager_.reset();
    client_receiver_.reset();
  }
  DCHECK(fake_device_manager);
  device_manager_.Bind(std::move(fake_device_manager));
  SetUpDeviceManagerConnection();
}
