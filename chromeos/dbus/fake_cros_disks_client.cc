// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cros_disks_client.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Performs fake mounting by creating a directory with a dummy file.
MountError PerformFakeMount(const std::string& source_path,
                            const base::FilePath& mounted_path,
                            MountType type) {
  if (mounted_path.empty())
    return MOUNT_ERROR_INVALID_ARGUMENT;

  // Just create an empty directory and shows it as the mounted directory.
  if (!base::CreateDirectory(mounted_path)) {
    DLOG(ERROR) << "Failed to create directory at " << mounted_path.value();
    return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
  }

  // Fake network mounts are responsible for populating their mount paths so
  // don't need a dummy file.
  if (type == MOUNT_TYPE_NETWORK_STORAGE)
    return MOUNT_ERROR_NONE;

  // Put a dummy file.
  const base::FilePath dummy_file_path =
      mounted_path.Append("SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt");
  const std::string dummy_file_content = "This is a dummy file.";
  const int write_result = base::WriteFile(
      dummy_file_path, dummy_file_content.data(), dummy_file_content.size());
  if (write_result != static_cast<int>(dummy_file_content.size())) {
    DLOG(ERROR) << "Failed to put a dummy file at "
                << dummy_file_path.value();
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }

  return MOUNT_ERROR_NONE;
}

}  // namespace

FakeCrosDisksClient::FakeCrosDisksClient() = default;

FakeCrosDisksClient::~FakeCrosDisksClient() = default;

void FakeCrosDisksClient::Init(dbus::Bus* bus) {
}

void FakeCrosDisksClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCrosDisksClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeCrosDisksClient::Mount(const std::string& source_path,
                                const std::string& source_format,
                                const std::string& mount_label,
                                const std::vector<std::string>& mount_options,
                                MountAccessMode access_mode,
                                RemountOption remount,
                                VoidDBusMethodCallback callback) {
  if (block_mount_) {
    return;
  }

  // This fake implementation assumes mounted path is device when source_format
  // is empty, or an archive otherwise.
  MountType type =
      source_format.empty() ? MOUNT_TYPE_DEVICE : MOUNT_TYPE_ARCHIVE;

  // Network storage source paths are URIs.
  if (GURL(source_path).is_valid())
    type = MOUNT_TYPE_NETWORK_STORAGE;

  base::FilePath mounted_path;
  switch (type) {
    case MOUNT_TYPE_ARCHIVE:
      mounted_path = GetArchiveMountPoint().Append(
          base::FilePath::FromUTF8Unsafe(mount_label));
      break;
    case MOUNT_TYPE_DEVICE:
      mounted_path = GetRemovableDiskMountPoint().Append(
          base::FilePath::FromUTF8Unsafe(mount_label));
      break;
    case MOUNT_TYPE_NETWORK_STORAGE:
      // Call all registered callbacks until mounted_path is non-empty.
      for (auto const& callback : custom_mount_point_callbacks_) {
        mounted_path = callback.Run(source_path, mount_options);
        if (!mounted_path.empty()) {
          break;
        }
      }
      break;
    case MOUNT_TYPE_INVALID:
      NOTREACHED();
      return;
  }
  mounted_paths_.insert(mounted_path);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PerformFakeMount, source_path, mounted_path, type),
      base::BindOnce(&FakeCrosDisksClient::DidMount,
                     weak_ptr_factory_.GetWeakPtr(), source_path, type,
                     mounted_path, std::move(callback)));
}

void FakeCrosDisksClient::DidMount(const std::string& source_path,
                                   MountType type,
                                   const base::FilePath& mounted_path,
                                   VoidDBusMethodCallback callback,
                                   MountError mount_error) {
  // Tell the caller of Mount() that the mount request was accepted.
  // Note that even if PerformFakeMount fails, this calls with |true| to
  // emulate the situation that 1) Mount operation is _successfully_ started,
  // 2) then failed for some reason.
  std::move(callback).Run(true);

  // Notify observers that the mount is completed.
  NotifyMountCompleted(mount_error, source_path, type,
                       mounted_path.AsUTF8Unsafe());
}

void FakeCrosDisksClient::Unmount(const std::string& device_path,
                                  UnmountCallback callback) {
  DCHECK(!callback.is_null());

  unmount_call_count_++;
  last_unmount_device_path_ = device_path;

  // Remove the dummy mounted directory if it exists.
  if (mounted_paths_.erase(base::FilePath::FromUTF8Unsafe(device_path))) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                       base::FilePath::FromUTF8Unsafe(device_path)),
        base::BindOnce(std::move(callback), unmount_error_));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), unmount_error_));
  }
  if (!unmount_listener_.is_null())
    unmount_listener_.Run();
}

void FakeCrosDisksClient::EnumerateDevices(EnumerateDevicesCallback callback,
                                           base::OnceClosure error_callback) {}

void FakeCrosDisksClient::EnumerateMountEntries(
    EnumerateMountEntriesCallback callback,
    base::OnceClosure error_callback) {}

void FakeCrosDisksClient::Format(const std::string& device_path,
                                 const std::string& filesystem,
                                 const std::string& label,
                                 VoidDBusMethodCallback callback) {
  DCHECK(!callback.is_null());

  format_call_count_++;
  last_format_device_path_ = device_path;
  last_format_filesystem_ = filesystem;
  last_format_label_ = label;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), format_success_));
}

void FakeCrosDisksClient::SinglePartitionFormat(const std::string& device_path,
                                                PartitionCallback callback) {
  DCHECK(!callback.is_null());

  partition_call_count_++;
  last_partition_device_path_ = device_path;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), partition_error_));
}

void FakeCrosDisksClient::Rename(const std::string& device_path,
                                 const std::string& volume_name,
                                 VoidDBusMethodCallback callback) {
  DCHECK(!callback.is_null());

  rename_call_count_++;
  last_rename_device_path_ = device_path;
  last_rename_volume_name_ = volume_name;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), rename_success_));
}

void FakeCrosDisksClient::GetDeviceProperties(
    const std::string& device_path,
    GetDevicePropertiesCallback callback,
    base::OnceClosure error_callback) {
  DCHECK(!callback.is_null());
  if (!next_get_device_properties_disk_info_ ||
      next_get_device_properties_disk_info_->device_path() != device_path) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(error_callback));
    return;
  }

  get_device_properties_success_count_++;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::cref(*next_get_device_properties_disk_info_)));
}

void FakeCrosDisksClient::NotifyMountCompleted(MountError error_code,
                                               const std::string& source_path,
                                               MountType mount_type,
                                               const std::string& mount_path) {
  for (auto& observer : observer_list_) {
    observer.OnMountCompleted(
        MountEntry(error_code, source_path, mount_type, mount_path));
  }
}

void FakeCrosDisksClient::NotifyFormatCompleted(
    FormatError error_code,
    const std::string& device_path) {
  for (auto& observer : observer_list_)
    observer.OnFormatCompleted(error_code, device_path);
}

void FakeCrosDisksClient::NotifyRenameCompleted(
    RenameError error_code,
    const std::string& device_path) {
  for (auto& observer : observer_list_)
    observer.OnRenameCompleted(error_code, device_path);
}

void FakeCrosDisksClient::NotifyMountEvent(MountEventType mount_event,
                                           const std::string& device_path) {
  for (auto& observer : observer_list_) {
    observer.OnMountEvent(mount_event, device_path);
  }
}

void FakeCrosDisksClient::AddCustomMountPointCallback(
    FakeCrosDisksClient::CustomMountPointCallback custom_mount_point_callback) {
  custom_mount_point_callbacks_.emplace_back(custom_mount_point_callback);
}

}  // namespace chromeos
