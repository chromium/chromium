// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_
#define CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"

class SingleFileTarReaderTest;

// SingleFileTarReader is a reader of tar archives with limited function. It
// only supports a tar archive with a single file entry.
// TODO(b/254591810): Reject an archive with multiple files.
class SingleFileTarReader {
 public:
  SingleFileTarReader();
  SingleFileTarReader(const SingleFileTarReader&) = delete;
  SingleFileTarReader& operator=(const SingleFileTarReader&) = delete;
  ~SingleFileTarReader();

  // Extracts a chunk of the tar file. To fully extract the file, the caller has
  // to repeatedly call this function until IsComplete() returns true.
  // Returns false if `src_buffer` is an invalid tar chunk. If not, `dst_buffer`
  // will point to a span within `src_buffer`.
  bool ExtractChunk(base::span<const uint8_t> src_buffer,
                    base::span<const uint8_t>& dst_buffer);

  bool IsComplete() const;

  // The return type uses uint64_t, because SingleFileTarReader supports large
  // files such as OS image files.
  std::optional<uint64_t> tar_content_size() const { return tar_content_size_; }
  uint64_t bytes_processed() const { return bytes_processed_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SingleFileTarReaderTest, ReadOctalNumber);

  // Read a number in Tar file header. It is normally a null-terminated octal
  // ASCII number but can be big-endian integer with padding when GNU extension
  // is used. The size of `buffer` must greater than 8.
  static std::optional<uint64_t> ReadOctalNumber(
      base::span<const uint8_t> buffer);

  // Populated once the size has been parsed. The value 0 means the file in
  // the tar is empty.
  std::optional<uint64_t> tar_content_size_;

  uint64_t bytes_processed_ = 0;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_
