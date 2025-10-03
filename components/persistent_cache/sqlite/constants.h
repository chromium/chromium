// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace persistent_cache::sqlite {

inline constexpr base::FilePath::CharType kDbFileExtension[] =
    FILE_PATH_LITERAL(".db");
inline constexpr base::FilePath::CharType kJournalFileExtension[] =
    FILE_PATH_LITERAL(".journal");

}  // namespace persistent_cache::sqlite

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_CONSTANTS_H_
