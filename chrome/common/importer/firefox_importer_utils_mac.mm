// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/firefox_importer_utils.h"

#include "base/files/file_util.h"
#include "base/path_service.h"

base::FilePath GetProfilesINI() {
  base::FilePath app_data_path;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_data_path)) {
    return base::FilePath();
  }
  base::FilePath ini_file =
      app_data_path.Append("Firefox").Append("profiles.ini");
  if (!base::PathExists(ini_file)) {
    return base::FilePath();
  }
  return ini_file;
}
