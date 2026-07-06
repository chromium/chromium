// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_TEST_SUPPORT_TEST_SQLITE_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_TEST_SUPPORT_TEST_SQLITE_UTILS_H_

namespace base {
class FilePath;
}  // namespace base

namespace storage {

// Corrupts the SQLite database at `database_path` by overwriting its file
// header with zero bytes while leaving the rest of the file intact.
void CorruptDatabaseHeaderForTesting(const base::FilePath& database_path);

// Drops the SQLite "meta" table at `database_path` while leaving the DOM
// Storage data tables intact.
void DropMetaTableForTesting(const base::FilePath& database_path);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_TEST_SUPPORT_TEST_SQLITE_UTILS_H_
