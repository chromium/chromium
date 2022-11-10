// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_
#define CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

constexpr int kDefaultBufferSize = 8192;

class SingleFileTarReaderTest;

// TODO(b/254591810): Remove
// chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h
// when tar file extraction moves from under
// chrome/browser/extensions/api/image_writer_private/ to
// chrome/services/file_util/
// TODO(b/254591810): Update SingleFileTarReader class for refactoring. For
// example SingleFileTarReader::Result can be bool after refactoring.

// SingleFileTarReader is a reader of tar archives with limited function. It
// only supports a tar archive with a single file entry. An archive with
// multiple files is rejected as error.
class SingleFileTarReader {
 public:
  enum class Result { kSuccess, kFailure, kShouldWait };

  // An interface that delegates file I/O of SingleFileTarReader.
  class Delegate {
   public:
    using Result = SingleFileTarReader::Result;

    virtual ~Delegate() = default;

    // Reads input data and returns kSuccess if it succeeds.
    // The input data will be written to `data`. `*size` is initially the size
    // of the `data` buffer. `*size` will be set to the amount actually read.
    // Returns kShouldWait if the data is still not available.
    // Returns kFailure and sets `error` if it fails.
    virtual Result ReadTarFile(
        char* data,
        uint32_t* size,
        chrome::file_util::mojom::ExtractionResult* error) = 0;

    // Writes the passed data. `size` is the size of the `data` buffer.
    // Returns false and sets `error` if it fails.
    virtual bool WriteContents(
        const char* data,
        int size,
        chrome::file_util::mojom::ExtractionResult* error) = 0;
  };

  explicit SingleFileTarReader(Delegate* delegate);
  SingleFileTarReader(const SingleFileTarReader&) = delete;
  SingleFileTarReader& operator=(const SingleFileTarReader&) = delete;
  ~SingleFileTarReader();

  // Extracts a chunk of the tar file. To fully extract the file, the caller has
  // to repeatedly call this function until IsComplete() returns true.
  // Returns kShouldWait if the input data is still not available. The caller
  // has to call ExtractChunk() again when the data is ready. The detail depends
  // on the implementation of the delegate.
  // Returns kFailure if it fails. error() identifies the reason of the
  // error.
  Result ExtractChunk();

  bool IsComplete() const;

  absl::optional<uint64_t> total_bytes() const { return total_bytes_; }
  uint64_t curr_bytes() const { return curr_bytes_; }

  chrome::file_util::mojom::ExtractionResult error() const { return error_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SingleFileTarReaderTest, ReadOctalNumber);

  // Read a number in Tar file header. It is normally a null-terminated octal
  // ASCII number but can be big-endian integer with padding when GNU extension
  // is used. `length` must greater than 8.
  static uint64_t ReadOctalNumber(const char* buffer, size_t length);

  const raw_ptr<Delegate> delegate_;

  // Populated once the size has been parsed. The value 0 means the file in
  // the tar is empty.
  absl::optional<uint64_t> total_bytes_;
  uint64_t curr_bytes_ = 0;

  std::vector<char> buffer_;

  chrome::file_util::mojom::ExtractionResult error_;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_READER_H_
