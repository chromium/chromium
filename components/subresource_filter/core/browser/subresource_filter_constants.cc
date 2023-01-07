// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

namespace subresource_filter {

const base::FilePath::CharType kTopLevelDirectoryName[] =
    FILE_PATH_LITERAL("Subresource Filter");

const base::FilePath::CharType kIndexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Indexed Rules");

const base::FilePath::CharType kUnindexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Unindexed Rules");

const base::FilePath::CharType kRulesetDataFileName[] =
    FILE_PATH_LITERAL("Ruleset Data");

const base::FilePath::CharType kLicenseFileName[] =
    FILE_PATH_LITERAL("LICENSE");

const base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("Indexing in Progress");

const base::FilePath::CharType kUnindexedRulesetLicenseFileName[] =
    FILE_PATH_LITERAL("LICENSE");

const base::FilePath::CharType kUnindexedRulesetDataFileName[] =
    FILE_PATH_LITERAL("Filtering Rules");

}  // namespace subresource_filter
