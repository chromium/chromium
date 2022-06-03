// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

#include "base/files/file_path.h"

namespace breadcrumbs {
namespace {

const base::FilePath::CharType kBreadcrumbsFile[] =
    FILE_PATH_LITERAL("Breadcrumbs");

const base::FilePath::CharType kBreadcrumbsTempFile[] =
    FILE_PATH_LITERAL("Breadcrumbs.temp");

}  // namespace

base::FilePath GetBreadcrumbPersistentStorageFilePath(
    const base::FilePath& storage_dir) {
  return storage_dir.Append(kBreadcrumbsFile);
}

base::FilePath GetBreadcrumbPersistentStorageTempFilePath(
    const base::FilePath& storage_dir) {
  return storage_dir.Append(kBreadcrumbsTempFile);
}

}  // namespace breadcrumbs
