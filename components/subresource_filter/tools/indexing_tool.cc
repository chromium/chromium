// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/indexing_tool.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/core/common/copying_file_stream.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {

bool IndexAndWriteRuleset(const base::FilePath& unindexed_path,
                          const base::FilePath& indexed_path,
                          int* out_checksum) {
  if (!base::PathExists(unindexed_path) ||
      !base::DirectoryExists(indexed_path.DirName())) {
    return false;
  }

  base::File unindexed_file(base::MakeAbsoluteFilePath(unindexed_path),
                            base::File::FLAG_OPEN | base::File::FLAG_READ);

  subresource_filter::RulesetIndexer indexer;

  CopyingFileInputStream copying_stream(std::move(unindexed_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream, 4096 /* buffer_size */);
  UnindexedRulesetReader reader(&zero_copy_stream_adaptor);

  url_pattern_index::proto::FilteringRules ruleset_chunk;

  while (reader.ReadNextChunk(&ruleset_chunk)) {
    for (const auto& rule : ruleset_chunk.url_rules()) {
      indexer.AddUrlRule(rule);
    }
  }

  indexer.Finish();

  base::WriteFile(indexed_path, indexer.data());

  if (out_checksum)
    *out_checksum = indexer.GetChecksum();

  return true;
}

void WriteVersionMetadata(const base::FilePath& path,
                          const std::string& content_version,
                          int checksum) {
  static constexpr char kVersionFormat[] = R"({
  "subresource_filter": {
    "ruleset_version": {
      "content": "%s",
      "format": %d,
      "checksum": %d
    }
  }
})";
  std::string version = base::StringPrintf(
      kVersionFormat, content_version.c_str(),
      subresource_filter::RulesetIndexer::kIndexedFormatVersion, checksum);
  base::WriteFile(path, version);
}

}  // namespace subresource_filter
