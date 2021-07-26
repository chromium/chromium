// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1231928): Define user data directory paths within the
// Chrome component namespace, or update UX to remove these concepts where they
// will not apply under Fuchsia.

#include "chrome/common/chrome_paths.h"

#include "base/files/file_path.h"
#include "base/notreached.h"

namespace chrome {

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  *result = base::FilePath("/data");
  return true;
}

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  *result = profile_dir;
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool GetUserDownloadsDirectorySafe(base::FilePath* result) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
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
