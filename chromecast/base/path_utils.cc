// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/path_utils.h"

#include "base/logging.h"
#include "base/path_service.h"

namespace chromecast {

namespace {

base::FilePath GetPath(base::BasePathKey default_dir_key,
                       const base::FilePath& path) {
  if (path.IsAbsolute())
    return path;

  base::FilePath default_dir;
  if (!base::PathService::Get(default_dir_key, &default_dir))
    LOG(DFATAL) << "Cannot get default dir: " << default_dir_key;

  base::FilePath adjusted_path(default_dir.Append(path));
  DVLOG(1) << "Path adjusted from " << path.value() << " to "
           << adjusted_path.value();
  return adjusted_path;
}

}  // namespace

base::FilePath GetHomePath(const base::FilePath& path) {
  return GetPath(base::DIR_HOME, path);
}

base::FilePath GetHomePathASCII(const std::string& path) {
  return GetHomePath(base::FilePath(path));
}

base::FilePath GetBinPath(const base::FilePath& path) {
  return GetPath(base::DIR_EXE, path);
}

base::FilePath GetBinPathASCII(const std::string& path) {
  return GetBinPath(base::FilePath(path));
}

base::FilePath GetTmpPath(const base::FilePath& path) {
  return GetPath(base::DIR_TEMP, path);
}

base::FilePath GetTmpPathASCII(const std::string& path) {
  return GetTmpPath(base::FilePath(path));
}

}  // namespace chromecast
