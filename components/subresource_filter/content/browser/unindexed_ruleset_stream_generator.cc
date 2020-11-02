// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/unindexed_ruleset_stream_generator.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "components/subresource_filter/content/browser/ruleset_version.h"
#include "components/subresource_filter/core/browser/copying_file_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

UnindexedRulesetStreamGenerator::UnindexedRulesetStreamGenerator(
    const UnindexedRulesetInfo& ruleset_info) {
  GenerateStreamFromFile(ruleset_info.ruleset_path);
}

UnindexedRulesetStreamGenerator::~UnindexedRulesetStreamGenerator() = default;

void UnindexedRulesetStreamGenerator::GenerateStreamFromFile(
    base::FilePath ruleset_path) {
  DCHECK(!ruleset_stream_);
  DCHECK(!copying_stream_);
  DCHECK_EQ(ruleset_size_, -1);

  base::File unindexed_ruleset_file(
      ruleset_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!unindexed_ruleset_file.IsValid()) {
    return;
  }

  ruleset_size_ = unindexed_ruleset_file.GetLength();

  copying_stream_ = std::make_unique<CopyingFileInputStream>(
      std::move(unindexed_ruleset_file));
  ruleset_stream_ =
      std::make_unique<google::protobuf::io::CopyingInputStreamAdaptor>(
          copying_stream_.get(), 4096 /* buffer_size */);
}

}  // namespace subresource_filter
