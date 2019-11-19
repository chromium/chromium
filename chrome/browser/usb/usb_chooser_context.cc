// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/cpp/usb/usb_ids.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // defined(OS_CHROMEOS)

namespace {

constexpr char kDeviceNameKey[] = "name";
constexpr char kGuidKey[] = "ephemeral-guid";
constexpr char kProductIdKey[] = "product-id";
constexpr char kSerialNumberKey[] = "serial-number";
constexpr char kVendorIdKey[] = "vendor-id";
constexpr int kDeviceIdWildcard = -1;

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
  return device_info.serial_number && !device_info.serial_number->empty();
}

std::pair<int, int> GetDeviceIds(const base::Value& object) {
  DCHECK(object.FindIntKey(kVendorIdKey));
  int vendor_id = *object.FindIntKey(kVendorIdKey);

  DCHECK(object.FindIntKey(kProductIdKey));
  int product_id = *object.FindIntKey(kProductIdKey);

  return std::make_pair(vendor_id, product_id);
}

base::string16 GetDeviceNameFromIds(int vendor_id, int product_id) {
#if !defined(OS_ANDROID)
  const char* product_name =
      device::UsbIds::GetProductName(vendor_id, product_id);
  if (product_name)
    return base::UTF8ToUTF16(product_name);

  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    if (product_id == kDeviceIdWildcard) {
      return l10n_util::GetStringFUTF16(IDS_DEVICE_DESCRIPTION_FOR_VENDOR_NAME,
                                        base::UTF8ToUTF16(vendor_name));
    }

    return l10n_util::GetStringFUTF16(
        IDS_DEVICE_DESCRIPTION_FOR_PRODUCT_ID_AND_VENDOR_NAME,
        base::ASCIIToUTF16(base::StringPrintf("0x%04X", product_id)),
        base::UTF8ToUTF16(vendor_name));
  }
#endif  // !defined(OS_ANDROID)

  if (product_id == kDeviceIdWildcard) {
    if (vendor_id == kDeviceIdWildcard)
      return l10n_util::GetStringUTF16(IDS_DEVICE_DESCRIPTION_FOR_ANY_VENDOR);

    return l10n_util::GetStringFUTF16(
        IDS_DEVICE_DESCRIPTION_FOR_VENDOR_ID,
        base::ASCIIToUTF16(base::StringPrintf("0x%04X", vendor_id)));
  }

  return l10n_util::GetStringFUTF16(
      IDS_DEVICE_DESCRIPTION_FOR_PRODUCT_ID_AND_VENDOR_ID,
      base::ASCIIToUTF16(base::StringPrintf("0x%04X", product_id)),
      base::ASCIIToUTF16(base::StringPrintf("0x%04X", vendor_id)));
}

base::Value DeviceIdsToValue(int vendor_id, int product_id) {
  base::Value device_value(base::Value::Type::DICTIONARY);
  base::string16 device_name = GetDeviceNameFromIds(vendor_id, product_id);

  device_value.SetStringKey(kDeviceNameKey, device_name);
  device_value.SetIntKey(kVendorIdKey, vendor_id);
  device_value.SetIntKey(kProductIdKey, product_id);
  device_value.SetStringKey(kSerialNumberKey, std::string());

  return device_value;
}

}  // namespace

void UsbChooserContext::DeviceObserver::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::DeviceObserver::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::DeviceObserver::OnDeviceManagerConnectionError() {}

UsbChooserContext::UsbChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         ContentSettingsType::USB_GUARD,
                         ContentSettingsType::USB_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()) {
#if defined(OS_CHROMEOS)
  bool is_signin_profile = chromeos::ProfileHelper::IsSigninProfile(profile);
  PrefService* pref_service = is_signin_profile
                                  ? g_browser_process->local_state()
                                  : profile->GetPrefs();
  const char* pref_name =
      is_signin_profile ? prefs::kDeviceLoginScreenWebUsbAllowDevicesForUrls
                        : prefs::kManagedWebUsbAllowDevicesForUrls;
#else   // defined(OS_CHROMEOS)
  PrefService* pref_service = profile->GetPrefs();
  const char* pref_name = prefs::kManagedWebUsbAllowDevicesForUrls;
#endif  // defined(OS_CHROMEOS)

  usb_policy_allowed_devices_.reset(
      new UsbPolicyAllowedDevices(pref_service, pref_name));
}

// static
base::Value UsbChooserContext::DeviceInfoToValue(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::Value device_value(base::Value::Type::DICTIONARY);
  device_value.SetStringKey(kDeviceNameKey, device_info.product_name
                                                ? *device_info.product_name
                                                : base::StringPiece16());
  device_value.SetIntKey(kVendorIdKey, device_info.vendor_id);
  device_value.SetIntKey(kProductIdKey, device_info.product_id);

  // CanStorePersistentEntry checks if |device_info.serial_number| is not empty.
  if (CanStorePersistentEntry(device_info)) {
    device_value.SetStringKey(kSerialNumberKey, *device_info.serial_number);
  } else {
    device_value.SetStringKey(kGuidKey, device_info.guid);
  }

  return device_value;
}

void UsbChooserContext::InitDeviceList(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    devices_.insert(std::make_pair(device_info->guid, std::move(device_info)));
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

void UsbChooserContext::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Receive mojo::Remote<UsbDeviceManager> from DeviceService.
  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName,
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

#if defined(OS_ANDROID)
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
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
UsbChooserContext::GetGrantedObjects(const url::Origin& requesting_origin,
                                     const url::Origin& embedding_origin) {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetGrantedObjects(requesting_origin,
                                            embedding_origin);

  if (CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      for (const std::string& guid : it->second) {
        // |devices_| should be initialized when |ephemeral_devices_| is filled.
        // Because |ephemeral_devices_| is filled by GrantDevicePermission()
        // which is called in UsbChooserController::Select(), this method will
        // always be called after device initialization in UsbChooserController
        // which always returns after the device list initialization in this
        // class.
        DCHECK(base::Contains(devices_, guid));
        objects.push_back(std::make_unique<ChooserContextBase::Object>(
            requesting_origin, embedding_origin,
            DeviceInfoToValue(*devices_[guid]),
            content_settings::SettingSource::SETTING_SOURCE_USER,
            is_incognito_));
      }
    }
  }

  // Iterate through the user granted objects and create a mapping of device IDs
  // to device object if the object is also allowed by policy. Any objects that
  // have been granted by policy are removed from |objects| to avoid duplicate
  // permissions from being displayed.
  // TODO(https://crbug.com/926984): This logic is very similar to the logic for
  // GetAllGrantedObjects(), so it could potentially be centralized.
  std::map<std::pair<int, int>, base::Value> device_ids_to_object_map;
  for (auto it = objects.begin(); it != objects.end();) {
    base::Value& object = (*it)->value;
    auto device_ids = GetDeviceIds(object);

    if (usb_policy_allowed_devices_->IsDeviceAllowed(
            requesting_origin, embedding_origin, device_ids)) {
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

    for (const auto& url_pair : allowed_devices_entry.second) {
      // Skip entries that do not match the |requesting_origin|.
      if (url_pair.first != requesting_origin)
        continue;

      // Skip entries that have a non-empty embedding origin that does not match
      // the given |embedding_origin|.
      if (url_pair.second && url_pair.second != embedding_origin) {
        continue;
      }

      // If there is an entry for the device in |device_ids_to_object_map|, use
      // that object to represent the device. Otherwise, attempt to figure out
      // the name of the device from the |vendor_id| and |product_id|.
      base::Value object(base::Value::Type::DICTIONARY);
      auto it =
          device_ids_to_object_map.find(std::make_pair(vendor_id, product_id));
      if (it != device_ids_to_object_map.end()) {
        object = std::move(it->second);
      } else {
        object = DeviceIdsToValue(vendor_id, product_id);
      }

      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          url_pair.first, url_pair.second, std::move(object),
          content_settings::SETTING_SOURCE_POLICY, is_incognito_));
    }
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
UsbChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetAllGrantedObjects();

  for (const auto& map_entry : ephemeral_devices_) {
    const url::Origin& requesting_origin = map_entry.first.first;
    const url::Origin& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const std::string& guid : map_entry.second) {
      DCHECK(base::Contains(devices_, guid));
      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          requesting_origin, embedding_origin,
          DeviceInfoToValue(*devices_[guid]),
          content_settings::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  // Iterate through the user granted objects to create a mapping of device IDs
  // to device object for the policy granted objects to use, and remove
  // objects that have already been granted permission by the policy.
  // TODO(https://crbug.com/926984): This logic is very similar to the logic for
  // GetGrantedObjects(), so it could potentially be centralized.
  std::map<std::pair<int, int>, base::Value> device_ids_to_object_map;
  for (auto it = objects.begin(); it != objects.end();) {
    Object& object = **it;
    auto device_ids = GetDeviceIds(object.value);
    auto requesting_origin = url::Origin::Create(object.requesting_origin);
    auto embedding_origin = url::Origin::Create(object.embedding_origin);

    if (usb_policy_allowed_devices_->IsDeviceAllowed(
            requesting_origin, embedding_origin, device_ids)) {
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

    for (const auto& url_pair : allowed_devices_entry.second) {
      // If there is an entry for the device in |device_ids_to_object_map|, use
      // that object to represent the device. Otherwise, attempt to figure out
      // the name of the device from the |vendor_id| and |product_id|.
      base::Value object(base::Value::Type::DICTIONARY);
      auto it =
          device_ids_to_object_map.find(std::make_pair(vendor_id, product_id));
      if (it != device_ids_to_object_map.end()) {
        object = it->second.Clone();
      } else {
        object = DeviceIdsToValue(vendor_id, product_id);
      }

      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          url_pair.first, url_pair.second, std::move(object),
          content_settings::SettingSource::SETTING_SOURCE_POLICY,
          is_incognito_));
    }
  }

  return objects;
}

void UsbChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  const std::string* guid = object.FindStringKey(kGuidKey);

  if (!guid) {
    ChooserContextBase::RevokeObjectPermission(requesting_origin,
                                               embedding_origin, object);
    RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED);
    return;
  }

  auto it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (it != ephemeral_devices_.end()) {
    it->second.erase(*guid);
    if (it->second.empty())
      ephemeral_devices_.erase(it);
    NotifyPermissionRevoked(requesting_origin, embedding_origin);
  }

  RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED_EPHEMERAL);
}

void UsbChooserContext::GrantDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (CanStorePersistentEntry(device_info)) {
    GrantObjectPermission(requesting_origin, embedding_origin,
                          DeviceInfoToValue(device_info));
  } else {
    ephemeral_devices_[std::make_pair(requesting_origin, embedding_origin)]
        .insert(device_info.guid);
    NotifyPermissionChanged();
  }
}

bool UsbChooserContext::HasDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
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
      base::Contains(it->second, device_info.guid)) {
    return true;
  }

  std::vector<std::unique_ptr<ChooserContextBase::Object>> object_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  for (const auto& object : object_list) {
    const base::Value& device = object->value;
    DCHECK(IsValidObject(device));

    const int vendor_id = *device.FindIntKey(kVendorIdKey);
    const int product_id = *device.FindIntKey(kProductIdKey);
    const std::string* serial_number = device.FindStringKey(kSerialNumberKey);
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
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_list)));
}

void UsbChooserContext::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(guid, std::move(device_receiver),
                             std::move(device_client));
}

const device::mojom::UsbDeviceInfo* UsbChooserContext::GetDeviceInfo(
    const std::string& guid) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  return it == devices_.end() ? nullptr : it->second.get();
}

#if defined(OS_ANDROID)
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

bool UsbChooserContext::IsValidObject(const base::Value& object) {
  return object.is_dict() && object.DictSize() == 4 &&
         object.FindStringKey(kDeviceNameKey) &&
         object.FindIntKey(kVendorIdKey) && object.FindIntKey(kProductIdKey) &&
         (object.FindStringKey(kSerialNumberKey) ||
          object.FindStringKey(kGuidKey));
}

// static
std::string UsbChooserContext::GetObjectName(const base::Value& object) {
  const std::string* name = object.FindStringKey(kDeviceNameKey);
  DCHECK(name);
  if (name->empty()) {
    base::Optional<int> vendor_id = object.FindIntKey(kVendorIdKey);
    base::Optional<int> product_id = object.FindIntKey(kProductIdKey);
    DCHECK(vendor_id && product_id);
    return base::UTF16ToUTF8(GetDeviceNameFromIds(*vendor_id, *product_id));
  }
  return *name;
}

void UsbChooserContext::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
  // Update the device list.
  DCHECK(!base::Contains(devices_, device_info->guid));
  devices_.insert(std::make_pair(device_info->guid, device_info->Clone()));

  // Notify all observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceAdded(*device_info);
}

void UsbChooserContext::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
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

  std::vector<std::pair<url::Origin, url::Origin>> revoked_url_pairs;
  for (auto& map_entry : ephemeral_devices_) {
    if (map_entry.second.erase(device_info->guid) > 0)
      revoked_url_pairs.push_back(map_entry.first);
  }

  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    for (auto& url_pair : revoked_url_pairs) {
      observer.OnPermissionRevoked(url_pair.first, url_pair.second);
    }
  }
}

void UsbChooserContext::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_receiver_.reset();
  devices_.clear();
  is_initialized_ = false;

  // Store the revoked URLs to notify observers of the revoked permissions.
  std::vector<std::pair<url::Origin, url::Origin>> revoked_url_pairs;
  for (auto& map_entry : ephemeral_devices_)
    revoked_url_pairs.push_back(map_entry.first);
  ephemeral_devices_.clear();

  // Notify all device observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceManagerConnectionError();

  // Notify all permission observers.
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    for (auto& url_pair : revoked_url_pairs) {
      observer.OnPermissionRevoked(url_pair.first, url_pair.second);
    }
  }
}

void UsbChooserContext::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager) {
  DCHECK(!device_manager_);
  DCHECK(fake_device_manager);
  device_manager_.Bind(std::move(fake_device_manager));
  SetUpDeviceManagerConnection();
}
