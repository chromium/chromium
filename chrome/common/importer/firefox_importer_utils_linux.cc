// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/firefox_importer_utils.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

base::FilePath GetProfilesINI() {
  base::FilePath ini_file;
  // The default location of the profile folder containing user data is
  // under user HOME directory in .mozilla/firefox folder on Linux.
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  if (!home.empty()) {
    ini_file = home.Append(".mozilla/firefox/profiles.ini");
  }
  if (base::PathExists(ini_file))
    return ini_file;

  return base::FilePath();
}
