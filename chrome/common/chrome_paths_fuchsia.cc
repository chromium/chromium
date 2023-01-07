// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1231928): Define user data directory paths within the
// Chrome component namespace, or update UX to remove these concepts where they
// will not apply under Fuchsia.

#include "chrome/common/chrome_paths.h"

#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/notreached.h"
#include "base/path_service.h"

namespace chrome {
namespace {

constexpr char kDocumentsDir[] = "Documents";
constexpr char kDownloadsDir[] = "Downloads";

}  // namespace

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  *result = base::FilePath(base::kPersistedDataDirectoryPath);
  return true;
}

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  *result = profile_dir;

  base::FilePath user_data_dir;
  if (!base::PathService::Get(DIR_USER_DATA, &user_data_dir))
    return;
  base::FilePath cache_dir(base::kPersistedCacheDirectoryPath);
  if (!user_data_dir.AppendRelativePath(profile_dir, &cache_dir))
    return;

  *result = cache_dir;
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  if (!GetDefaultUserDataDirectory(result))
    return false;
  *result = result->Append(kDocumentsDir);
  return true;
}

bool GetUserDownloadsDirectorySafe(base::FilePath* result) {
  if (!GetDefaultUserDataDirectory(result))
    return false;
  *result = result->Append(kDownloadsDir);
  return true;
}

bool GetUserDownloadsDirectory(base::FilePath* result) {
  return GetUserDownloadsDirectorySafe(result);
}

bool GetUserMusicDirectory(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool GetUserPicturesDirectory(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool GetUserVideosDirectory(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  // Only the browser actually needs DIR_USER_DATA to be set.
  return process_type.empty();
}

}  // namespace chrome
