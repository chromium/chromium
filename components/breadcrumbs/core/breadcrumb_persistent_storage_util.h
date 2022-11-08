// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_

namespace base {
class FilePath;
}

namespace breadcrumbs {

// Returns the path to a file for storing breadcrumbs within |storage_dir|.
base::FilePath GetBreadcrumbPersistentStorageFilePath(
    const base::FilePath& storage_dir);

// Deletes the breadcrumbs file and breadcrumbs temp file in |storage_dir|.
void DeleteBreadcrumbFiles(const base::FilePath& storage_dir);

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_
