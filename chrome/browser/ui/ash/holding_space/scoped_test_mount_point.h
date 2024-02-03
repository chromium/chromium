// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_MOUNT_POINT_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_MOUNT_POINT_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace ash {
namespace holding_space {

// Utility class that registers the downloads external file system mount point,
// and grants file manager app access permission for the mount point.
class ScopedTestMountPoint {
 public:
  // Creates and mounts a mount point for downloads.
  static std::unique_ptr<ScopedTestMountPoint> CreateAndMountDownloads(
      Profile* profile);

  // Creates a mount point on the file system - the backing mount path existence
  // will be scoped to the `ScopedTestMountPoint` lifetime, but the mount point
  // will not be registered as an external mount point by default (so tests can
  // initialize mount point state before adding it as an external mount point).
  ScopedTestMountPoint(const std::string& name,
                       storage::FileSystemType file_system_type,
                       file_manager::VolumeType volume_type);
  ScopedTestMountPoint(const ScopedTestMountPoint&) = delete;
  ScopedTestMountPoint& operator=(const ScopedTestMountPoint&) = delete;
  ~ScopedTestMountPoint();

  // Mounts the mount point - registarts the mount point as an external system
  // mount point, and adds it to the file manager's volume manager.
  void Mount(Profile* profile);

  // Gets the mount point root path on the local file system.
  const base::FilePath& GetRootPath() const { return temp_dir_.GetPath(); }

  // Whether the mount point directory has been successfully created.
  bool IsValid() const;

  const std::string& name() const { return name_; }

  // Creates a file under [mount point root path]/|relative_path| with the
  // provided content. Returns the created file's file path, or an empty path on
  // failure.
  base::FilePath CreateFile(const base::FilePath& relative_path,
                            const std::string& content = std::string());

  // Creates an arbitrary file under the 'mount_point'.
  base::FilePath CreateArbitraryFile();

 private:
  std::string name_;
  const storage::FileSystemType file_system_type_;
  const file_manager::VolumeType volume_type_;

  raw_ptr<Profile> profile_ = nullptr;
  bool mounted_ = false;
  base::ScopedTempDir temp_dir_;
};

}  // namespace holding_space
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_MOUNT_POINT_H_
