// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_CONSTANTS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace storage {

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kWebStorageDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kLocalStoragePath[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const char kLocalStorageLeveldbName[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kServiceWorkerDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kMediaLicenseDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kMediaLicenseDatabaseFileName[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kFileSystemDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kIndexedDbDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kBackgroundFetchDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kCacheStorageDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kScriptCacheDirectory[];

COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
extern const base::FilePath::CharType kSharedStoragePath[];

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_CONSTANTS_H_
