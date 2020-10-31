// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_DOWNLOADS_MOUNT_POINT_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_DOWNLOADS_MOUNT_POINT_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace ash {
namespace holding_space {

// Utility class that registers the downloads external file system mount point,
// and grants file manager app access permission for the mount point.
class ScopedTestDownloadsMountPoint {
 public:
  explicit ScopedTestDownloadsMountPoint(Profile* profile);
  ScopedTestDownloadsMountPoint(const ScopedTestDownloadsMountPoint&) = delete;
  ScopedTestDownloadsMountPoint& operator=(
      const ScopedTestDownloadsMountPoint&) = delete;
  ~ScopedTestDownloadsMountPoint();

  bool IsValid() const { return temp_dir_.IsValid(); }

  const base::FilePath& GetRootPath() const { return temp_dir_.GetPath(); }

  const std::string& name() const { return name_; }

  // Creates a file under [mount point root path]/|relative_path| with the
  // provided content. Returns the created file's file path, or an empty path on
  // failure.
  base::FilePath CreateFile(const base::FilePath& relative_path,
                            const std::string& content);

  // Creates an arbitrary file under the 'mount_point'.
  base::FilePath CreateArbitraryFile();

 private:
  base::ScopedTempDir temp_dir_;
  std::string name_;
};

}  // namespace holding_space
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_SCOPED_TEST_DOWNLOADS_MOUNT_POINT_H_