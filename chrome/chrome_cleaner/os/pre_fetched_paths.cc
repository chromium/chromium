// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

#include <shlobj.h>

#include "base/base_paths_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

// static
PreFetchedPaths* PreFetchedPaths::GetInstance() {
  return base::Singleton<PreFetchedPaths>::get();
}

PreFetchedPaths::~PreFetchedPaths() = default;

// Pre-fetches all paths and returns true if all fetching operations were
// successful.
bool PreFetchedPaths::Initialize() {
  DCHECK(!initialized_);
  chrome_cleaner::InitializeFilePathSanitization();

  initialized_ =
      FetchPath(base::FILE_EXE) && FetchPath(base::DIR_PROGRAM_FILES) &&
      FetchPath(base::DIR_WINDOWS) && FetchPath(base::DIR_COMMON_APP_DATA) &&
      FetchPath(base::DIR_LOCAL_APP_DATA) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_PROGRAM_FILES)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_PROGRAM_FILESX86)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_WINDOWS)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_STARTUP)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_SYSTEM)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_COMMON_APPDATA)) &&
      FetchPath(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  return initialized_;
}

void PreFetchedPaths::DisableForTesting() {
  cache_disabled_ = true;
}

base::FilePath PreFetchedPaths::GetExecutablePath() const {
  return Get(base::FILE_EXE);
}

base::FilePath PreFetchedPaths::GetProgramFilesFolder() const {
  return Get(base::DIR_PROGRAM_FILES);
}

base::FilePath PreFetchedPaths::GetWindowsFolder() const {
  return Get(base::DIR_WINDOWS);
}

base::FilePath PreFetchedPaths::GetCommonAppDataFolder() const {
  return Get(base::DIR_COMMON_APP_DATA);
}

base::FilePath PreFetchedPaths::GetLocalAppDataFolder() const {
  return Get(base::DIR_LOCAL_APP_DATA);
}

base::FilePath PreFetchedPaths::GetCsidlProgramFilesFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_PROGRAM_FILES));
}

base::FilePath PreFetchedPaths::GetCsidlProgramFilesX86Folder() const {
  return Get(CsidlToPathServiceKey(CSIDL_PROGRAM_FILESX86));
}

base::FilePath PreFetchedPaths::GetCsidlWindowsFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_WINDOWS));
}

base::FilePath PreFetchedPaths::GetCsidlStartupFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_STARTUP));
}

base::FilePath PreFetchedPaths::GetCsidlSystemFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_SYSTEM));
}

base::FilePath PreFetchedPaths::GetCsidlCommonAppDataFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_COMMON_APPDATA));
}

base::FilePath PreFetchedPaths::GetCsidlLocalAppDataFolder() const {
  return Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
}

PreFetchedPaths::PreFetchedPaths() = default;

bool PreFetchedPaths::FetchPath(int key) {
  base::FilePath path;
  if (base::PathService::Get(key, &path) && !path.empty()) {
    paths_[key] = NormalizePath(path);
    return true;
  }

  LOG(ERROR) << "Cannot retrieve file path for key " << key;
  return false;
}

base::FilePath PreFetchedPaths::Get(int key) const {
  if (!cache_disabled_) {
    CHECK(initialized_);
    DCHECK(paths_.count(key));
    return paths_.at(key);
  }

  base::FilePath path;
  const bool success = base::PathService::Get(key, &path);
  CHECK(success && !path.empty());
  return path;
}

}  // namespace chrome_cleaner
