// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_INDEXING_TOOL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_INDEXING_TOOL_H_

#include "base/command_line.h"
#include "base/files/file_path.h"

namespace subresource_filter {

// Given |unindexed_path|, which is a path to an unindexed ruleset, writes the
// indexed (flatbuffer) version to |indexed_path|. Returns false if there was
// something wrong with the given paths. If successful, stores the checksum of
// the ruleset in the outparam.
bool IndexAndWriteRuleset(const base::FilePath& unindexed_path,
                          const base::FilePath& indexed_path,
                          int* out_checksum = nullptr);

// Write version JSON to |path|. This matches the version JSON found in
// preferences.
void WriteVersionMetadata(const base::FilePath& path,
                          const std::string& content_version,
                          int checksum);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_INDEXING_TOOL_H_
