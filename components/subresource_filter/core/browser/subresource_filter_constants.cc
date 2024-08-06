// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

#include <string_view>

#include "base/files/file_path.h"

namespace subresource_filter {

constexpr base::FilePath::CharType kIndexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Indexed Rules");

constexpr base::FilePath::CharType kUnindexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Unindexed Rules");

constexpr base::FilePath::CharType kRulesetDataFileName[] =
    FILE_PATH_LITERAL("Ruleset Data");

constexpr base::FilePath::CharType kLicenseFileName[] =
    FILE_PATH_LITERAL("LICENSE");

constexpr base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("Indexing in Progress");

constexpr base::FilePath::CharType kUnindexedRulesetLicenseFileName[] =
    FILE_PATH_LITERAL("LICENSE");

constexpr base::FilePath::CharType kUnindexedRulesetDataFileName[] =
    FILE_PATH_LITERAL("Filtering Rules");

}  // namespace subresource_filter
