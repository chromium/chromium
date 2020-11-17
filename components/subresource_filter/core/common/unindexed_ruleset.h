// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Semantically speaking, an unindexed ruleset consists of a single
// url_pattern_index::proto::FilteringRules message. However, because the
// ruleset can be relatively large, we want to avoid deserializing all of it at
// once, as doing so can lead to memory allocation bursts.
//
// To work around the limitation that partial (or streaming) deserialization is
// not supported by the proto parser, the UnindexedRulesetReader/Writer classes
// implement a format where the ruleset is split into several chunks. Each chunk
// is itself a url_pattern_index::proto::FilteringRules message. If all chunks
// were merged, they would add up to the original
// url_pattern_index::proto::FilteringRules message representing the entire
// ruleset.
//
// Consumers of an unindexed ruleset in this format will be able to read it one
// chunk at a time, and are expected to fully process and discard the chunk
// before reading the next one. In practice, this should not be an issue as
// indexing of the ruleset is expected to be performed in an on-line fashion.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_UNINDEXED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_UNINDEXED_RULESET_H_

#include "base/macros.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"

namespace subresource_filter {

// Reads an unindexed ruleset from |stream| one chunk at a time.
class UnindexedRulesetReader {
 public:
  // Note: The |stream| should outlive |this| instance.
  explicit UnindexedRulesetReader(
      google::protobuf::io::ZeroCopyInputStream* stream);
  ~UnindexedRulesetReader();

  // Reads the next ruleset |chunk| from the |input|. Returns false iff reached
  // the end of the stream or an error occurred. Once returned false, calling
  // ReadNextChunk is undefined befaviour.
  bool ReadNextChunk(url_pattern_index::proto::FilteringRules* chunk);

  // Returns how many bytes of the |stream| have been consumed.
  int num_bytes_read() const { return coded_stream_.CurrentPosition(); }

 private:
  google::protobuf::io::CodedInputStream coded_stream_;

  DISALLOW_COPY_AND_ASSIGN(UnindexedRulesetReader);
};

// Divides an unindexed ruleset into chunks and writes them into |stream|.
//
// Writing methods of this class return bool false if an I/O error occurrs
// during these calls. In this case the UnindexedRulesetWriter becomes broken,
// and write operations should not be used further.
class UnindexedRulesetWriter {
 public:
  // Creates an instance that will write
  // url_pattern_index::proto::FilteringRules chunks to the |stream|, with each
  // chunk containing up to |max_rules_per_chunk| rules.
  explicit UnindexedRulesetWriter(
      google::protobuf::io::ZeroCopyOutputStream* stream,
      int max_rules_per_chunk = 64);
  ~UnindexedRulesetWriter();

  int max_rules_per_chunk() const { return max_rules_per_chunk_; }

  // Returns whether an I/O error occurred since this object was created.
  bool had_error() const { return coded_stream_.HadError(); }

  // Places the |rule| to the current chunk, and serializes the chunk if it has
  // grown up to |max_rules_per_chunk|.
  bool AddUrlRule(const url_pattern_index::proto::UrlRule& rule);
  // TODO(pkalinnikov): Implement AddCssRule when needed.

  // Finalizes the serialization of the unindexed ruleset, i.e., writes the
  // final chunk of rules, if there are any still pending. This method *should*
  // be called exactly once when interaction with |this| instance ends, unless
  // some of the writing operations returned false, which indicates an error.
  // After calling Finish write operations should not be used any more.
  bool Finish();

 private:
  // Writes the non-empty |pending_chunk_| to the |coded_stream_|. Always leaves
  // the |panding_chunk_| empty, regardless of whether an error occurred.
  bool WritePendingChunk();

  google::protobuf::io::CodedOutputStream coded_stream_;

  const int max_rules_per_chunk_ = 0;
  url_pattern_index::proto::FilteringRules pending_chunk_;

  DISALLOW_COPY_AND_ASSIGN(UnindexedRulesetWriter);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_UNINDEXED_RULESET_H_
