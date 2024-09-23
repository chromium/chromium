// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Any tasks that communicates with the portable device may take >100ms to
// complete. Those tasks should be run on an blocking thread instead of the
// UI thread.

#include "components/storage_monitor/portable_device_watcher_win.h"

#include <objbase.h>

#include <dbt.h>
#include <portabledevice.h>
#include <wrl/client.h>

#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/browser_thread.h"

namespace storage_monitor {

namespace {

// Name of the client application that communicates with the MTP device.
const wchar_t kClientName[] = L"Chromium";

// Returns true if |data| represents a class of portable devices.
bool IsPortableDeviceStructure(LPARAM data) {
  DEV_BROADCAST_HDR* broadcast_hdr =
      reinterpret_cast<DEV_BROADCAST_HDR*>(data);
  if (!broadcast_hdr ||
      (broadcast_hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)) {
    return false;
  }

  GUID guidDevInterface = GUID_NULL;
  if (FAILED(CLSIDFromString(kWPDDevInterfaceGUID, &guidDevInterface)))
    return false;
  DEV_BROADCAST_DEVICEINTERFACE* dev_interface =
      reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(data);
  return (IsEqualGUID(dev_interface->dbcc_classguid, guidDevInterface) != 0);
}

// Returns the portable device plug and play device ID string.
std::wstring GetPnpDeviceId(LPARAM data) {
  DEV_BROADCAST_DEVICEINTERFACE* dev_interface =
      reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(data);
  if (!dev_interface)
    return std::wstring();
  std::wstring device_id(dev_interface->dbcc_name);
  DCHECK(base::IsStringASCII(device_id));
  return base::ToLowerASCII(device_id);
}

// Gets the friendly name of the device specified by the |pnp_device_id|. On
// success, returns true and fills in |name|.
bool GetFriendlyName(const std::wstring& pnp_device_id,
                     IPortableDeviceManager* device_manager,
                     std::wstring* name) {
  DCHECK(device_manager);
  DCHECK(name);
  DWORD name_len = 0;
  HRESULT hr = device_manager->GetDeviceFriendlyName(pnp_device_id.c_str(),
                                                     nullptr, &name_len);
  if (FAILED(hr))
    return false;

  hr = device_manager->GetDeviceFriendlyName(
      pnp_device_id.c_str(), base::WriteInto(name, name_len), &name_len);
  return (SUCCEEDED(hr) && !name->empty());
}

// Gets the manufacturer name of the device specified by the |pnp_device_id|.
// On success, returns true and fills in |name|.
bool GetManufacturerName(const std::wstring& pnp_device_id,
                         IPortableDeviceManager* device_manager,
                         std::wstring* name) {
  DCHECK(device_manager);
  DCHECK(name);
  DWORD name_len = 0;
  HRESULT hr = device_manager->GetDeviceManufacturer(pnp_device_id.c_str(),
                                                     nullptr, &name_len);
  if (FAILED(hr))
    return false;

  hr = device_manager->GetDeviceManufacturer(pnp_device_id.c_str(),
                                             base::WriteInto(name, name_len),
                                             &name_len);
  return (SUCCEEDED(hr) && !name->empty());
}

// Gets the description of the device specified by the |pnp_device_id|. On
// success, returns true and fills in |description|.
bool GetDeviceDescription(const std::wstring& pnp_device_id,
                          IPortableDeviceManager* device_manager,
                          std::wstring* description) {
  DCHECK(device_manager);
  DCHECK(description);
  DWORD desc_len = 0;
  HRESULT hr = device_manager->GetDeviceDescription(pnp_device_id.c_str(),
                                                    nullptr, &desc_len);
  if (FAILED(hr))
    return false;

  hr = device_manager->GetDeviceDescription(
      pnp_device_id.c_str(), base::WriteInto(description, desc_len), &desc_len);
  return (SUCCEEDED(hr) && !description->empty());
}

// On success, returns true and updates |client_info| with a reference to an
// IPortableDeviceValues interface that holds information about the
// application that communicates with the device.
bool GetClientInformation(
    Microsoft::WRL::ComPtr<IPortableDeviceValues>* client_info) {
  HRESULT hr =
      ::CoCreateInstance(__uuidof(PortableDeviceValues), nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&(*client_info)));
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to create an instance of IPortableDeviceValues";
    return false;
  }

  // Attempt to set client details.
  (*client_info)->SetStringValue(WPD_CLIENT_NAME, kClientName);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0);
  (*client_info)->SetUnsignedIntegerValue(
      WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS,
                                          GENERIC_READ);
  return true;
}

// Opens the device for communication. |pnp_device_id| specifies the plug and
// play device ID string. On success, returns true and updates |device| with a
// reference to the portable device interface.
bool SetUp(const std::wstring& pnp_device_id,
           Microsoft::WRL::ComPtr<IPortableDevice>* device) {
  Microsoft::WRL::ComPtr<IPortableDeviceValues> client_info;
  if (!GetClientInformation(&client_info))
    return false;

  HRESULT hr =
      ::CoCreateInstance(__uuidof(PortableDevice), nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&(*device)));
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to create an instance of IPortableDevice";
    return false;
  }

  hr = (*device)->Open(pnp_device_id.c_str(), client_info.Get());
  if (SUCCEEDED(hr))
    return true;

  if (hr == E_ACCESSDENIED)
    DPLOG(ERROR) << "Access denied to open the device";
  return false;
}

// Returns the unique id property key of the object specified by the
// |object_id|.
REFPROPERTYKEY GetUniqueIdPropertyKey(const std::wstring& object_id) {
  return (object_id == WPD_DEVICE_OBJECT_ID) ?
      WPD_DEVICE_SERIAL_NUMBER : WPD_OBJECT_PERSISTENT_UNIQUE_ID;
}

// On success, returns true and populates |properties_to_read| with the
// property key of the object specified by the |object_id|.
bool PopulatePropertyKeyCollection(
    const std::wstring& object_id,
    Microsoft::WRL::ComPtr<IPortableDeviceKeyCollection>* properties_to_read) {
  HRESULT hr = ::CoCreateInstance(__uuidof(PortableDeviceKeyCollection),
                                  nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&(*properties_to_read)));
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to create IPortableDeviceKeyCollection instance";
    return false;
  }
  REFPROPERTYKEY key = GetUniqueIdPropertyKey(object_id);
  hr = (*properties_to_read)->Add(key);
  return SUCCEEDED(hr);
}

// Wrapper function to get content property string value.
bool GetStringPropertyValue(IPortableDeviceValues* properties_values,
                            REFPROPERTYKEY key,
                            std::wstring* value) {
  DCHECK(properties_values);
  DCHECK(value);
  base::win::ScopedCoMem<wchar_t> buffer;
  HRESULT hr = properties_values->GetStringValue(key, &buffer);
  if (FAILED(hr))
    return false;
  *value = static_cast<const wchar_t*>(buffer);
  return true;
}

// Constructs a unique identifier for the object specified by the |object_id|.
// On success, returns true and fills in |unique_id|.
bool GetObjectUniqueId(IPortableDevice* device,
                       const std::wstring& object_id,
                       std::wstring* unique_id) {
  DCHECK(device);
  DCHECK(unique_id);
  Microsoft::WRL::ComPtr<IPortableDeviceContent> content;
  HRESULT hr = device->Content(&content);
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to get IPortableDeviceContent interface";
    return false;
  }

  Microsoft::WRL::ComPtr<IPortableDeviceProperties> properties;
  hr = content->Properties(&properties);
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to get IPortableDeviceProperties interface";
    return false;
  }

  Microsoft::WRL::ComPtr<IPortableDeviceKeyCollection> properties_to_read;
  if (!PopulatePropertyKeyCollection(object_id, &properties_to_read))
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceValues> properties_values;
  if (FAILED(properties->GetValues(object_id.c_str(), properties_to_read.Get(),
                                   &properties_values))) {
    return false;
  }

  REFPROPERTYKEY key = GetUniqueIdPropertyKey(object_id);
  return GetStringPropertyValue(properties_values.Get(), key, unique_id);
}

// Constructs the device storage unique identifier using |device_serial_num| and
// |storage_id|. On success, returns true and fills in |device_storage_id|.
bool ConstructDeviceStorageUniqueId(const std::wstring& device_serial_num,
                                    const std::wstring& storage_id,
                                    std::string* device_storage_id) {
  if (device_serial_num.empty() && storage_id.empty())
    return false;

  DCHECK(device_storage_id);
  *device_storage_id = StorageInfo::MakeDeviceId(
      StorageInfo::MTP_OR_PTP,
      base::WideToUTF8(storage_id + L':' + device_serial_num));
  return true;
}

// Gets a list of removable storage object identifiers present in |device|.
// On success, returns true and fills in |storage_object_ids|.
bool GetRemovableStorageObjectIds(
    IPortableDevice* device,
    PortableDeviceWatcherWin::StorageObjectIDs* storage_object_ids) {
  DCHECK(device);
  DCHECK(storage_object_ids);
  Microsoft::WRL::ComPtr<IPortableDeviceCapabilities> capabilities;
  HRESULT hr = device->Capabilities(&capabilities);
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to get IPortableDeviceCapabilities interface";
    return false;
  }

  Microsoft::WRL::ComPtr<IPortableDevicePropVariantCollection> storage_ids;
  hr = capabilities->GetFunctionalObjects(WPD_FUNCTIONAL_CATEGORY_STORAGE,
                                          &storage_ids);
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to get IPortableDevicePropVariantCollection";
    return false;
  }

  DWORD num_storage_obj_ids = 0;
  hr = storage_ids->GetCount(&num_storage_obj_ids);
  if (FAILED(hr))
    return false;

  for (DWORD index = 0; index < num_storage_obj_ids; ++index) {
    base::win::ScopedPropVariant object_id;
    hr = storage_ids->GetAt(index, object_id.Receive());
    if (SUCCEEDED(hr) && object_id.get().vt == VT_LPWSTR &&
        object_id.get().pwszVal != nullptr) {
      storage_object_ids->push_back(object_id.get().pwszVal);
    }
  }
  return true;
}

// Returns true if the portable device belongs to a mass storage class.
// |pnp_device_id| specifies the plug and play device id.
// |device_name| specifies the name of the device.
bool IsMassStoragePortableDevice(const std::wstring& pnp_device_id,
                                 const std::wstring& device_name) {
  // Based on testing, if the pnp device id starts with "\\?\wpdbusenumroot#",
  // then the attached device belongs to a mass storage class.
  if (base::StartsWith(pnp_device_id, L"\\\\?\\wpdbusenumroot#",
                       base::CompareCase::INSENSITIVE_ASCII))
    return true;

  // If the device is a volume mounted device, |device_name| will be
  // the volume name.
  return ((device_name.length() >= 2) && (device_name[1] == L':') &&
      (((device_name[0] >= L'A') && (device_name[0] <= L'Z')) ||
          ((device_name[0] >= L'a') && (device_name[0] <= L'z'))));
}

// Returns the name of the device specified by |pnp_device_id|.
std::wstring GetDeviceNameOnBlockingThread(
    IPortableDeviceManager* portable_device_manager,
    const std::wstring& pnp_device_id) {
  DCHECK(portable_device_manager);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::wstring name;
  GetFriendlyName(pnp_device_id, portable_device_manager, &name) ||
      GetDeviceDescription(pnp_device_id, portable_device_manager, &name) ||
      GetManufacturerName(pnp_device_id, portable_device_manager, &name);
  return name;
}

// Access the device and gets the device storage details. On success, returns
// true and populates |storage_objects| with device storage details.
bool GetDeviceStorageObjectsOnBlockingThread(
    const std::wstring& pnp_device_id,
    PortableDeviceWatcherWin::StorageObjects* storage_objects) {
  DCHECK(storage_objects);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Microsoft::WRL::ComPtr<IPortableDevice> device;
  if (!SetUp(pnp_device_id, &device))
    return false;

  std::wstring device_serial_num;
  if (!GetObjectUniqueId(device.Get(), WPD_DEVICE_OBJECT_ID,
                         &device_serial_num)) {
    return false;
  }

  PortableDeviceWatcherWin::StorageObjectIDs storage_obj_ids;
  if (!GetRemovableStorageObjectIds(device.Get(), &storage_obj_ids))
    return false;
  for (PortableDeviceWatcherWin::StorageObjectIDs::const_iterator id_iter =
       storage_obj_ids.begin(); id_iter != storage_obj_ids.end(); ++id_iter) {
    std::wstring storage_persistent_id;
    if (!GetObjectUniqueId(device.Get(), *id_iter, &storage_persistent_id))
      continue;

    std::string device_storage_id;
    if (ConstructDeviceStorageUniqueId(device_serial_num, storage_persistent_id,
                                       &device_storage_id)) {
      storage_objects->push_back(PortableDeviceWatcherWin::DeviceStorageObject(
          *id_iter, device_storage_id));
    }
  }
  return true;
}

// Accesses the device and gets the device details (name, storage info, etc).
// On success returns true and fills in |device_details|. On failure, returns
// false. |pnp_device_id| specifies the plug and play device ID string.
bool GetDeviceInfoOnBlockingThread(
    IPortableDeviceManager* portable_device_manager,
    const std::wstring& pnp_device_id,
    PortableDeviceWatcherWin::DeviceDetails* device_details) {
  DCHECK(portable_device_manager);
  DCHECK(device_details);
  DCHECK(!pnp_device_id.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  device_details->name = GetDeviceNameOnBlockingThread(portable_device_manager,
                                                       pnp_device_id);
  if (IsMassStoragePortableDevice(pnp_device_id, device_details->name))
    return false;

  device_details->location = pnp_device_id;
  PortableDeviceWatcherWin::StorageObjects storage_objects;
  return GetDeviceStorageObjectsOnBlockingThread(
      pnp_device_id, &device_details->storage_objects);
}

// Wrapper function to get an instance of portable device manager. On success,
// returns true and fills in |portable_device_mgr|. On failure, returns false.
bool GetPortableDeviceManager(
    Microsoft::WRL::ComPtr<IPortableDeviceManager>* portable_device_mgr) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  HRESULT hr = ::CoCreateInstance(__uuidof(PortableDeviceManager), nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&(*portable_device_mgr)));
  if (SUCCEEDED(hr))
    return true;

  // Either there is no portable device support (Windows XP with old versions of
  // Media Player) or the thread does not have COM initialized.
  DCHECK_NE(CO_E_NOTINITIALIZED, hr);
  return false;
}

// Enumerates the attached portable devices. On success, returns true and fills
// in |devices| with the attached portable device details. On failure, returns
// false.
bool EnumerateAttachedDevicesOnBlockingThread(
    PortableDeviceWatcherWin::Devices* devices) {
  DCHECK(devices);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Microsoft::WRL::ComPtr<IPortableDeviceManager> portable_device_mgr;
  if (!GetPortableDeviceManager(&portable_device_mgr))
    return false;

  // Get the total number of devices found on the system.
  DWORD pnp_device_count = 0;
  HRESULT hr = portable_device_mgr->GetDevices(nullptr, &pnp_device_count);
  if (FAILED(hr))
    return false;

  auto pnp_device_ids = base::HeapArray<wchar_t*>::Uninit(pnp_device_count);
  hr =
      portable_device_mgr->GetDevices(pnp_device_ids.data(), &pnp_device_count);
  if (FAILED(hr))
    return false;

  for (DWORD index = 0; index < pnp_device_count; ++index) {
    PortableDeviceWatcherWin::DeviceDetails device_details;
    if (GetDeviceInfoOnBlockingThread(portable_device_mgr.Get(),
                                      pnp_device_ids[index], &device_details))
      devices->push_back(device_details);
    CoTaskMemFree(pnp_device_ids[index]);
  }
  return !devices->empty();
}

// Handles the device attach event message on a media task runner.
// |pnp_device_id| specifies the attached plug and play device ID string. On
// success, returns true and populates |device_details| with device information.
// On failure, returns false.
bool HandleDeviceAttachedEventOnBlockingThread(
    const std::wstring& pnp_device_id,
    PortableDeviceWatcherWin::DeviceDetails* device_details) {
  DCHECK(device_details);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Microsoft::WRL::ComPtr<IPortableDeviceManager> portable_device_mgr;
  if (!GetPortableDeviceManager(&portable_device_mgr))
    return false;
  // Sometimes, portable device manager doesn't have the new device details.
  // Refresh the manager device list to update its details.
  portable_device_mgr->RefreshDeviceList();
  return GetDeviceInfoOnBlockingThread(portable_device_mgr.Get(), pnp_device_id,
                                       device_details);
}

// Registers |hwnd| to receive portable device notification details. On success,
// returns the device notifications handle else returns NULL.
HDEVNOTIFY RegisterPortableDeviceNotification(HWND hwnd) {
  GUID dev_interface_guid = GUID_NULL;
  HRESULT hr = CLSIDFromString(kWPDDevInterfaceGUID, &dev_interface_guid);
  if (FAILED(hr))
    return nullptr;
  DEV_BROADCAST_DEVICEINTERFACE db = {
      sizeof(DEV_BROADCAST_DEVICEINTERFACE),
      DBT_DEVTYP_DEVICEINTERFACE,
      0,
      dev_interface_guid
  };
  return RegisterDeviceNotification(hwnd, &db, DEVICE_NOTIFY_WINDOW_HANDLE);
}

}  // namespace


// PortableDeviceWatcherWin ---------------------------------------------------

PortableDeviceWatcherWin::DeviceStorageObject::DeviceStorageObject(
    const std::wstring& temporary_id,
    const std::string& persistent_id)
    : object_temporary_id(temporary_id), object_persistent_id(persistent_id) {}

PortableDeviceWatcherWin::DeviceDetails::DeviceDetails() {
}

PortableDeviceWatcherWin::DeviceDetails::DeviceDetails(
    const DeviceDetails& other) = default;

PortableDeviceWatcherWin::DeviceDetails::~DeviceDetails() {
}

PortableDeviceWatcherWin::PortableDeviceWatcherWin()
    : notifications_(nullptr), storage_notifications_(nullptr) {}

PortableDeviceWatcherWin::~PortableDeviceWatcherWin() {
  UnregisterDeviceNotification(notifications_);
}

void PortableDeviceWatcherWin::Init(HWND hwnd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notifications_ = RegisterPortableDeviceNotification(hwnd);
  media_task_runner_ = base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  EnumerateAttachedDevices();
}

void PortableDeviceWatcherWin::OnWindowMessage(UINT event_type, LPARAM data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsPortableDeviceStructure(data))
    return;

  std::wstring device_id = GetPnpDeviceId(data);
  if (event_type == DBT_DEVICEARRIVAL)
    HandleDeviceAttachEvent(device_id);
  else if (event_type == DBT_DEVICEREMOVECOMPLETE)
    HandleDeviceDetachEvent(device_id);
}

bool PortableDeviceWatcherWin::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    std::wstring* device_location,
    std::wstring* storage_object_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(device_location);
  DCHECK(storage_object_id);
  MTPStorageMap::const_iterator storage_map_iter =
      storage_map_.find(storage_device_id);
  if (storage_map_iter == storage_map_.end())
    return false;

  MTPDeviceMap::const_iterator device_iter =
      device_map_.find(storage_map_iter->second.location());
  if (device_iter == device_map_.end())
    return false;
  const StorageObjects& storage_objects = device_iter->second;
  for (StorageObjects::const_iterator storage_object_iter =
       storage_objects.begin(); storage_object_iter != storage_objects.end();
       ++storage_object_iter) {
    if (storage_device_id == storage_object_iter->object_persistent_id) {
      *device_location = storage_map_iter->second.location();
      *storage_object_id = storage_object_iter->object_temporary_id;
      return true;
    }
  }
  return false;
}

// static
std::wstring PortableDeviceWatcherWin::GetStoragePathFromStorageId(
    const std::string& storage_unique_id) {
  // Construct a dummy device path using the storage name. This is only used
  // for registering the device media file system.
  DCHECK(!storage_unique_id.empty());
  return base::UTF8ToWide("\\\\" + storage_unique_id);
}

void PortableDeviceWatcherWin::SetNotifications(
    StorageMonitor::Receiver* notifications) {
  storage_notifications_ = notifications;
}

void PortableDeviceWatcherWin::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback) {
  // MTP devices on Windows don't have a detach API needed -- signal
  // the object as if the device is gone and tell the caller it is OK
  // to remove.
  std::wstring device_location;  // The device_map_ key.
  std::wstring storage_object_id;
  if (!GetMTPStorageInfoFromDeviceId(device_id,
                                     &device_location, &storage_object_id)) {
    std::move(callback).Run(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }
  HandleDeviceDetachEvent(device_location);

  std::move(callback).Run(StorageMonitor::EJECT_OK);
}

void PortableDeviceWatcherWin::EnumerateAttachedDevices() {
  DCHECK(media_task_runner_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Devices* devices = new Devices;
  media_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EnumerateAttachedDevicesOnBlockingThread, devices),
      base::BindOnce(&PortableDeviceWatcherWin::OnDidEnumerateAttachedDevices,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(devices)));
}

void PortableDeviceWatcherWin::OnDidEnumerateAttachedDevices(
    const Devices* devices, const bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(devices);
  if (!result)
    return;
  for (Devices::const_iterator device_iter = devices->begin();
       device_iter != devices->end(); ++device_iter) {
    OnDidHandleDeviceAttachEvent(&(*device_iter), result);
  }
}

void PortableDeviceWatcherWin::HandleDeviceAttachEvent(
    const std::wstring& pnp_device_id) {
  DCHECK(media_task_runner_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DeviceDetails* device_details = new DeviceDetails;
  media_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HandleDeviceAttachedEventOnBlockingThread, pnp_device_id,
                     device_details),
      base::BindOnce(&PortableDeviceWatcherWin::OnDidHandleDeviceAttachEvent,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(device_details)));
}

void PortableDeviceWatcherWin::OnDidHandleDeviceAttachEvent(
    const DeviceDetails* device_details, const bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(device_details);
  if (!result)
    return;

  const StorageObjects& storage_objects = device_details->storage_objects;
  const std::wstring& name = device_details->name;
  const std::wstring& location = device_details->location;
  DCHECK(!base::Contains(device_map_, location));
  for (StorageObjects::const_iterator storage_iter = storage_objects.begin();
       storage_iter != storage_objects.end(); ++storage_iter) {
    const std::string& storage_id = storage_iter->object_persistent_id;
    DCHECK(!base::Contains(storage_map_, storage_id));

    if (storage_id.empty() || name.empty())
      return;

    // Device can have several data partitions. Therefore, add the
    // partition identifier to the model name. E.g.: "Nexus 7 (s10001)"
    std::wstring model_name(name + L" (" + storage_iter->object_temporary_id +
                            L')');
    StorageInfo info(storage_id, location, std::u16string(), std::u16string(),
                     base::WideToUTF16(model_name), 0);
    storage_map_[storage_id] = info;
    if (storage_notifications_) {
      info.set_location(GetStoragePathFromStorageId(storage_id));
      storage_notifications_->ProcessAttach(info);
    }
  }
  device_map_[location] = storage_objects;
}

void PortableDeviceWatcherWin::HandleDeviceDetachEvent(
    const std::wstring& pnp_device_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceMap::iterator device_iter = device_map_.find(pnp_device_id);
  if (device_iter == device_map_.end())
    return;

  const StorageObjects& storage_objects = device_iter->second;
  for (StorageObjects::const_iterator storage_object_iter =
       storage_objects.begin(); storage_object_iter != storage_objects.end();
       ++storage_object_iter) {
    std::string storage_id = storage_object_iter->object_persistent_id;
    MTPStorageMap::iterator storage_map_iter = storage_map_.find(storage_id);
    CHECK(storage_map_iter != storage_map_.end(), base::NotFatalUntil::M130);
    if (storage_notifications_) {
      storage_notifications_->ProcessDetach(
          storage_map_iter->second.device_id());
    }
    storage_map_.erase(storage_map_iter);
  }
  device_map_.erase(device_iter);
}

}  // namespace storage_monitor
