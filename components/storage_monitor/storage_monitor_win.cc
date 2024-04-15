// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_win.h"

#include <windows.h>

#include <dbt.h>
#include <fileapi.h>
#include <shlobj.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"
#include "components/storage_monitor/portable_device_watcher_win.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/volume_mount_watcher_win.h"

#define WM_USER_MEDIACHANGED (WM_USER + 5)

// StorageMonitorWin -------------------------------------------------------

namespace storage_monitor {

StorageMonitorWin::StorageMonitorWin(
    std::unique_ptr<VolumeMountWatcherWin> volume_mount_watcher,
    std::unique_ptr<PortableDeviceWatcherWin> portable_device_watcher)
    : volume_mount_watcher_(std::move(volume_mount_watcher)),
      portable_device_watcher_(std::move(portable_device_watcher)) {
  DCHECK(volume_mount_watcher_);
  DCHECK(portable_device_watcher_);
  volume_mount_watcher_->SetNotifications(receiver());
  portable_device_watcher_->SetNotifications(receiver());
}

StorageMonitorWin::~StorageMonitorWin() {
  if (shell_change_notify_id_)
    SHChangeNotifyDeregister(shell_change_notify_id_);
  volume_mount_watcher_->SetNotifications(nullptr);
  portable_device_watcher_->SetNotifications(nullptr);

  if (window_)
    DestroyWindow(window_);

  if (window_class_)
    UnregisterClass(MAKEINTATOM(window_class_), instance_);
}

void StorageMonitorWin::Init() {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      L"Chrome_StorageMonitorWindow",
      &base::win::WrappedWindowProc<StorageMonitorWin::WndProcThunk>, 0, 0, 0,
      nullptr, nullptr, nullptr, nullptr, nullptr, &window_class);
  instance_ = window_class.hInstance;
  window_class_ = RegisterClassEx(&window_class);
  DCHECK(window_class_);

  window_ = CreateWindow(MAKEINTATOM(window_class_), nullptr, 0, 0, 0, 0, 0,
                         nullptr, nullptr, instance_, nullptr);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  volume_mount_watcher_->Init();
  portable_device_watcher_->Init(window_);
  MediaChangeNotificationRegister();
}

bool StorageMonitorWin::GetStorageInfoForPath(const base::FilePath& path,
                                              StorageInfo* device_info) const {
  DCHECK(device_info);

  // TODO(gbillock): Move this logic up to StorageMonitor.
  // If we already know the StorageInfo for the path, just return it.
  // This will account for portable devices as well.
  std::vector<StorageInfo> attached_devices = GetAllAvailableStorages();
  size_t best_parent = attached_devices.size();
  size_t best_length = 0;
  for (size_t i = 0; i < attached_devices.size(); i++) {
    if (!StorageInfo::IsRemovableDevice(attached_devices[i].device_id()))
      continue;
    base::FilePath relative;
    if (base::FilePath(attached_devices[i].location()).AppendRelativePath(
            path, &relative)) {
      // Note: the relative path is longer for shorter shared path between
      // the path and the device mount point, so we want the shortest
      // relative path.
      if (relative.value().size() < best_length) {
        best_parent = i;
        best_length = relative.value().size();
      }
    }
  }
  if (best_parent != attached_devices.size()) {
    *device_info = attached_devices[best_parent];
    return true;
  }

  return GetDeviceInfo(path, device_info);
}

void StorageMonitorWin::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(EjectStatus)> callback) {
  StorageInfo::Type type;
  if (!StorageInfo::CrackDeviceId(device_id, &type, nullptr)) {
    std::move(callback).Run(EJECT_FAILURE);
    return;
  }

  if (type == StorageInfo::MTP_OR_PTP)
    portable_device_watcher_->EjectDevice(device_id, std::move(callback));
  else if (StorageInfo::IsRemovableDevice(device_id))
    volume_mount_watcher_->EjectDevice(device_id, std::move(callback));
  else
    std::move(callback).Run(EJECT_FAILURE);
}

bool StorageMonitorWin::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    std::wstring* device_location,
    std::wstring* storage_object_id) const {
  StorageInfo::Type type;
  StorageInfo::CrackDeviceId(storage_device_id, &type, nullptr);
  if (type != StorageInfo::MTP_OR_PTP)
    return false;
  return portable_device_watcher_->GetMTPStorageInfoFromDeviceId(
      storage_device_id, device_location, storage_object_id);
}

// static
LRESULT CALLBACK StorageMonitorWin::WndProcThunk(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam) {
  StorageMonitorWin* msg_wnd = reinterpret_cast<StorageMonitorWin*>(
      GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg_wnd)
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK StorageMonitorWin::WndProc(HWND hwnd, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_DEVICECHANGE:
      OnDeviceChange(static_cast<UINT>(wparam), lparam);
      return TRUE;
    case WM_USER_MEDIACHANGED:
      OnMediaChange(wparam, lparam);
      return TRUE;
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

void StorageMonitorWin::MediaChangeNotificationRegister() {
  LPITEMIDLIST id_list;
  if (SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &id_list) == NOERROR) {
    SHChangeNotifyEntry notify_entry;
    notify_entry.pidl = id_list;
    notify_entry.fRecursive = TRUE;
    shell_change_notify_id_ = SHChangeNotifyRegister(
        window_, SHCNRF_ShellLevel, SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED,
        WM_USER_MEDIACHANGED, 1, &notify_entry);
    if (!shell_change_notify_id_)
      DVLOG(1) << "SHChangeNotifyRegister FAILED";
  } else {
    DVLOG(1) << "SHGetSpecialFolderLocation FAILED";
  }
}

bool StorageMonitorWin::GetDeviceInfo(const base::FilePath& device_path,
                                      StorageInfo* info) const {
  DCHECK(info);

  // TODO(kmadhusu) Implement PortableDeviceWatcherWin::GetDeviceInfo()
  // function when we have the functionality to add a sub directory of
  // portable device as a media gallery.
  return volume_mount_watcher_->GetDeviceInfo(device_path, info);
}

void StorageMonitorWin::OnDeviceChange(UINT event_type, LPARAM data) {
  DVLOG(1) << "OnDeviceChange " << event_type << " " << data;
  volume_mount_watcher_->OnWindowMessage(event_type, data);
  portable_device_watcher_->OnWindowMessage(event_type, data);
}

void StorageMonitorWin::OnMediaChange(WPARAM wparam, LPARAM lparam) {
  volume_mount_watcher_->OnMediaChange(wparam, lparam);
}

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorWin(std::make_unique<VolumeMountWatcherWin>(),
                               std::make_unique<PortableDeviceWatcherWin>());
}

}  // namespace storage_monitor
