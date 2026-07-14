// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include <optional>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"

namespace chrome {

namespace {

// Returns the relative path to append to DIR_CACHE for the given `profile_dir`,
// or std::nullopt if the cache should be located directly under DIR_CACHE.
//
// The returned path is determined as follows:
// - For the initial (Default) profile itself: Returns std::nullopt to keep its
//   cache directly under DIR_CACHE for backward compatibility.
// - For isolated storage partitions under the initial profile: Returns the
//   path relative to the profile (e.g., "Storage/ext/glic").
// - For non-initial profiles (typically used in tests) and their partitions:
//   Returns the path relative to the user data directory (e.g.,
//   "profile_name" or "profile_name/Storage/ext/glic"), ensuring cache
//   isolation between different profiles.
std::optional<base::FilePath> GetCacheRelativePath(
    const base::FilePath& profile_dir) {
  CHECK(!profile_dir.empty());

  const auto user_data_dir = base::PathService::CheckedGet(DIR_USER_DATA);

  base::FilePath relative_path;
  if (!user_data_dir.AppendRelativePath(profile_dir, &relative_path)) {
    // In some unit tests, the profile directory might be outside the user data
    // directory. In this case, we don't try to partition the cache.
    return std::nullopt;
  }

  const auto components = relative_path.GetComponents();
  CHECK(!components.empty());
  if (components[0] != kInitialProfile) {
    // For non-initial profiles, use the path relative to the user data
    // directory (which starts with the profile name) to isolate their caches.
    return relative_path;
  }

  const auto active_profile_dir = user_data_dir.Append(kInitialProfile);
  if (profile_dir == active_profile_dir) {
    // The initial profile itself (not its isolated storage partitions) keeps
    // its cache directly under DIR_CACHE for backward compatibility.
    return std::nullopt;
  }

  // For isolated partitions under the initial profile, return the relative path
  // from the profile directory (e.g., "Storage/ext/glic") to append to
  // DIR_CACHE.
  base::FilePath relative_to_profile;
  CHECK(
      active_profile_dir.AppendRelativePath(profile_dir, &relative_to_profile));
  return relative_to_profile;
}

}  // namespace

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  if (base::PathService::Get(base::DIR_CACHE, result)) {
    // Append the relative path of the partition to the cache directory to
    // prevent collision.
    if (base::FeatureList::IsEnabled(
            features::kAndroidKeepProfilePartitionDirsInCacheDir)) {
      if (const auto relative_path = GetCacheRelativePath(profile_dir)) {
        *result = result->Append(*relative_path);
      }
    }
  } else {
    *result = profile_dir;
  }
}

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  return base::PathService::Get(base::DIR_ANDROID_APP_DATA, result);
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  if (!GetDefaultUserDataDirectory(result))
    return false;
  *result = result->Append("Documents");
  return true;
}

bool GetUserDownloadsDirectory(base::FilePath* result) {
  if (!GetDefaultUserDataDirectory(result))
    return false;
  *result = result->Append("Downloads");
  return true;
}

bool GetUserMusicDirectory(base::FilePath* result) {
  NOTIMPLEMENTED();
  return false;
}

bool GetUserPicturesDirectory(base::FilePath* result) {
  NOTIMPLEMENTED();
  return false;
}

bool GetUserVideosDirectory(base::FilePath* result) {
  NOTIMPLEMENTED();
  return false;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  // SELinux prohibits accessing the data directory from isolated services. Only
  // the browser (empty process type) should access the profile directory.
  return process_type.empty();
}

}  // namespace chrome
