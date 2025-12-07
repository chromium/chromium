// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/unindexed_ruleset.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

// UnindexedRulesetReader ------------------------------------------------------

UnindexedRulesetReader::UnindexedRulesetReader(
    google::protobuf::io::ZeroCopyInputStream* stream)
    : coded_stream_(stream) {}

UnindexedRulesetReader::~UnindexedRulesetReader() = default;

bool UnindexedRulesetReader::ReadNextChunk(proto::FilteringRules* chunk) {
  uint32_t chunk_size = 0;
  if (!coded_stream_.ReadVarint32(&chunk_size)) {
    return false;
  }
  auto limit = coded_stream_.PushLimit(chunk_size);
  if (!chunk->ParseFromCodedStream(&coded_stream_)) {
    return false;
  }
  coded_stream_.PopLimit(limit);
  return true;
}

// UnindexedRulesetWriter ------------------------------------------------------

UnindexedRulesetWriter::UnindexedRulesetWriter(
    google::protobuf::io::ZeroCopyOutputStream* stream,
    int max_rules_per_chunk)
    : coded_stream_(stream), max_rules_per_chunk_(max_rules_per_chunk) {}

UnindexedRulesetWriter::~UnindexedRulesetWriter() {
  CHECK_EQ(pending_chunk_.url_rules_size(), 0);
  CHECK_EQ(pending_chunk_.css_rules_size(), 0);
}

bool UnindexedRulesetWriter::AddUrlRule(const proto::UrlRule& rule) {
  CHECK(!had_error());
  pending_chunk_.add_url_rules()->CopyFrom(rule);
  if (pending_chunk_.url_rules_size() >= max_rules_per_chunk_) {
    CHECK_EQ(pending_chunk_.url_rules_size(), max_rules_per_chunk_);
    return WritePendingChunk();
  }
  return true;
}

bool UnindexedRulesetWriter::Finish() {
  CHECK(!had_error());
  const bool success = !pending_chunk_.url_rules_size() || WritePendingChunk();
  if (success) {
    coded_stream_.Trim();
  }
  return success;
}

bool UnindexedRulesetWriter::WritePendingChunk() {
  CHECK(!had_error());
  CHECK_GT(pending_chunk_.url_rules_size(), 0);

  proto::FilteringRules chunk;
  chunk.Swap(&pending_chunk_);
  coded_stream_.WriteVarint32(
      base::checked_cast<uint32_t>(chunk.ByteSizeLong()));
  return !had_error() && chunk.SerializeToCodedStream(&coded_stream_);
}

}  // namespace subresource_filter
