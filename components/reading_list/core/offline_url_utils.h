// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_OFFLINE_URL_UTILS_H_
#define COMPONENTS_READING_LIST_CORE_OFFLINE_URL_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace reading_list {

// The different types of file that can be stored for offline usage.
enum OfflineFileType { OFFLINE_TYPE_HTML = 0, OFFLINE_TYPE_PDF = 1 };

// The absolute path of the directory where offline URLs are saved.
// |profile_path| is the path to the profile directory that contain the offline
// directory.
base::FilePath OfflineRootDirectoryPath(const base::FilePath& profile_path);

// The absolute path of |relative_path| where |relative_path| is relative to the
// offline root folder (OfflineRootDirectoryPath).
// |profile_path| is the path to the profile directory that contain the offline
// directory.
// The file/directory may not exist.
base::FilePath OfflineURLAbsolutePathFromRelativePath(
    const base::FilePath& profile_path,
    const base::FilePath& relative_path);

// The absolute path of the directory where a |url| is saved offline.
// Contains the page and supporting files (images).
// |profile_path| is the path to the profile directory that contain the offline
// directory.
// The directory may not exist.
base::FilePath OfflineURLDirectoryAbsolutePath(
    const base::FilePath& profile_path,
    const GURL& url);

// The relative path to the offline webpage for the |url|. The result is
// relative to |OfflineRootDirectoryPath()|. |type| is the type of the file and
// will determine the extension of the returned value.
// The file may not exist.
base::FilePath OfflinePagePath(const GURL& url, OfflineFileType type);

// The name of the directory containing offline data for |url|.
std::string OfflineURLDirectoryID(const GURL& url);

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_OFFLINE_URL_UTILS_H_
