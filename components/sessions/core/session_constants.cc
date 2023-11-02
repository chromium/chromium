// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_constants.h"

namespace sessions {

const base::FilePath::StringPieceType kSessionsDirectory =
    FILE_PATH_LITERAL("Sessions");

const base::FilePath::CharType kTabSessionFileNamePrefix[] =
    FILE_PATH_LITERAL("Tabs");

const base::FilePath::CharType kSessionFileNamePrefix[] =
    FILE_PATH_LITERAL("Session");

const base::FilePath::CharType kAppSessionFileNamePrefix[] =
    FILE_PATH_LITERAL("Apps");

const base::FilePath::CharType kTimestampSeparator[] = FILE_PATH_LITERAL("_");

const base::FilePath::StringPieceType kLegacyCurrentTabSessionFileName =
    FILE_PATH_LITERAL("Current Tabs");
const base::FilePath::StringPieceType kLegacyLastTabSessionFileName =
    FILE_PATH_LITERAL("Last Tabs");

const base::FilePath::StringPieceType kLegacyCurrentSessionFileName =
    FILE_PATH_LITERAL("Current Session");
const base::FilePath::StringPieceType kLegacyLastSessionFileName =
    FILE_PATH_LITERAL("Last Session");

const int gMaxPersistNavigationCount = 6;

}  // namespace sessions
