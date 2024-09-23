// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UNINDEXED_RULESET_STREAM_GENERATOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UNINDEXED_RULESET_STREAM_GENERATOR_H_

#include <stdint.h>

#include <memory>
#include <sstream>

namespace base {
class FilePath;
}

namespace google {
namespace protobuf {
namespace io {
class ZeroCopyInputStream;
}
}  // namespace protobuf
}  // namespace google

namespace subresource_filter {

class CopyingFileInputStream;
struct UnindexedRulesetInfo;

// Processes the on-disk representation of the unindexed ruleset data into a
// stream via which a client can read this data.
class UnindexedRulesetStreamGenerator {
 public:
  explicit UnindexedRulesetStreamGenerator(
      const UnindexedRulesetInfo& ruleset_info);
  ~UnindexedRulesetStreamGenerator();

  UnindexedRulesetStreamGenerator(const UnindexedRulesetStreamGenerator&) =
      delete;
  UnindexedRulesetStreamGenerator& operator=(
      const UnindexedRulesetStreamGenerator&) = delete;

  // Returns a ZeroCopyInputStream* via which the unindexed ruleset data can be
  // streamed. If the returned pointer is null, the stream is not valid.
  // NOTE: The returned pointer will be valid only for the lifetime of this
  // object.
  google::protobuf::io::ZeroCopyInputStream* ruleset_stream() {
    return ruleset_stream_.get();
  }

  // Returns the size of the unindexed ruleset data in bytes.
  // If the size is < 0, the stream is not valid.
  int64_t ruleset_size() const { return ruleset_size_; }

 private:
  // Generates |ruleset_stream_| from the file at |ruleset_path_|.
  void GenerateStreamFromFile(base::FilePath ruleset_path);

  // Generates |ruleset_stream_| from the contents of the string stored in the
  // resource bundle at |resource_id|.
  void GenerateStreamFromResourceId(int resource_id);

  int64_t ruleset_size_ = -1;

  // Used when the stream is generated from a file on disk.
  std::unique_ptr<CopyingFileInputStream> copying_stream_;

  // Used when the stream is generated from a resource ID.
  std::istringstream string_stream_;

  // The stream via which a client of this class can read the data of the
  // unindexed ruleset.
  std::unique_ptr<google::protobuf::io::ZeroCopyInputStream> ruleset_stream_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UNINDEXED_RULESET_STREAM_GENERATOR_H_
