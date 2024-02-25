// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_CONSTANTS_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_CONSTANTS_H_

#include "base/files/file_path.h"

namespace affiliations {

inline constexpr base::FilePath::CharType kAffiliationDatabaseFileName[] =
    FILE_PATH_LITERAL("Affiliation Database");

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_CONSTANTS_H_
