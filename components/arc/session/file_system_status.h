// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_
#define COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_

#include "base/files/file_path.h"

namespace arc {

// A move-only class to hold status of the host file system. This class is for
// ArcVmClientAdapter's internal use and visible for only testing purposes. Do
// not use directly.
class FileSystemStatus {
 public:
  FileSystemStatus(FileSystemStatus&& other);
  ~FileSystemStatus();
  FileSystemStatus& operator=(FileSystemStatus&& other);

  FileSystemStatus(const FileSystemStatus&) = delete;
  FileSystemStatus& operator=(const FileSystemStatus&) = delete;

  static FileSystemStatus GetFileSystemStatusBlocking() {
    return FileSystemStatus();
  }

  bool is_android_debuggable() const { return is_android_debuggable_; }
  bool is_host_rootfs_writable() const { return is_host_rootfs_writable_; }
  bool is_system_image_ext_format() const {
    return is_system_image_ext_format_;
  }
  const base::FilePath& system_image_path() const { return system_image_path_; }
  const base::FilePath& vendor_image_path() const { return vendor_image_path_; }
  const base::FilePath& guest_kernel_path() const { return guest_kernel_path_; }
  const base::FilePath& fstab_path() const { return fstab_path_; }

  // Setters for testing.
  void set_android_debuggable_for_testing(bool is_android_debuggable) {
    is_android_debuggable_ = is_android_debuggable;
  }
  void set_host_rootfs_writable_for_testing(bool is_host_rootfs_writable) {
    is_host_rootfs_writable_ = is_host_rootfs_writable;
  }
  void set_system_image_ext_format_for_testing(
      bool is_system_image_ext_format) {
    is_system_image_ext_format_ = is_system_image_ext_format;
  }
  void set_system_image_path_for_testing(
      const base::FilePath& system_image_path) {
    system_image_path_ = system_image_path;
  }
  void set_vendor_image_path_for_testing(
      const base::FilePath& vendor_image_path) {
    vendor_image_path_ = vendor_image_path;
  }
  void set_guest_kernel_path_for_testing(
      const base::FilePath& guest_kernel_path) {
    guest_kernel_path_ = guest_kernel_path;
  }
  void set_fstab_path_for_testing(const base::FilePath& fstab_path) {
    fstab_path_ = fstab_path;
  }

  static bool IsAndroidDebuggableForTesting(const base::FilePath& json_path) {
    return IsAndroidDebuggable(json_path);
  }
  static bool IsSystemImageExtFormatForTesting(const base::FilePath& path) {
    return IsSystemImageExtFormat(path);
  }

 private:
  FileSystemStatus();

  // Parse a JSON file which is like the following and returns a result:
  //   {
  //     "ANDROID_DEBUGGABLE": false
  //   }
  static bool IsAndroidDebuggable(const base::FilePath& json_path);

  static bool IsHostRootfsWritable();

  // https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
  // Super block starts from block 0, offset 0x400.
  // 0x38: Magic signature (Len=16, value=0xEF53) in little-endian order.
  static bool IsSystemImageExtFormat(const base::FilePath& path);

  bool is_android_debuggable_;
  bool is_host_rootfs_writable_;
  base::FilePath system_image_path_;
  base::FilePath vendor_image_path_;
  base::FilePath guest_kernel_path_;
  base::FilePath fstab_path_;
  bool is_system_image_ext_format_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_
