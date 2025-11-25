// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace persistent_cache::sqlite {

// TODO(crbug.com/377475540): Migrate from ".db" to no extension for the DB file
// and use "-journal" and "-wal" for the others for consistency with SQLite.
inline constexpr base::FilePath::CharType kDbFileExtension[] =
    FILE_PATH_LITERAL(".db");
inline constexpr base::FilePath::CharType kJournalFileExtension[] =
    FILE_PATH_LITERAL(".journal");
inline constexpr base::FilePath::CharType kWalJournalFileExtension[] =
    FILE_PATH_LITERAL(".db-wal");

}  // namespace persistent_cache::sqlite

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_
