// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <optional>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/memory/raw_span.h"

namespace safe_browsing {
namespace dmg {

// An interface for reading and seeking over a byte stream.
class ReadStream {
 public:
  virtual ~ReadStream() {}

  // Copies up to |buf.size()| bytes from the stream into |buf|.
  // Returns |true| on success with the actual number of bytes written stored in
  // |bytes_read| (which should be non-null). Returns |false| on error. At
  // end-of-stream, returns |true| with |bytes_read| set to 0.
  virtual bool Read(base::span<uint8_t> buf, size_t* bytes_read) = 0;

  // Calls |Read| but only returns true if the number of bytes read equals
  // |buf.size()|.
  bool ReadExact(base::span<uint8_t> buf);

  // Reads bytes corresponding to exactly one T instance from the stream.
  template <typename T>
  bool ReadType(T& t) {
    return ReadExact(base::byte_span_from_ref(t));
  }

  // Seeks the read stream to |offset| from |whence|. |whence| is a POSIX-style
  // SEEK_ constant. Returns the resulting offset location, or a negative value
  // on failure.
  virtual off_t Seek(off_t offset, int whence) = 0;
};

// An implementation of ReadStream backed by a file descriptor. This does not
// take ownership of the file descriptor.
class FileReadStream : public ReadStream {
 public:
  explicit FileReadStream(int fd);

  FileReadStream(const FileReadStream&) = delete;
  FileReadStream& operator=(const FileReadStream&) = delete;

  ~FileReadStream() override;

  // ReadStream:
  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override;
  off_t Seek(off_t offset, int whence) override;

 private:
  int fd_;
};

// An implementation of ReadStream that operates on a byte buffer. This class
// does not take ownership of the underlying byte array.
class MemoryReadStream : public ReadStream {
 public:
  explicit MemoryReadStream(base::span<const uint8_t> byte_buf);

  MemoryReadStream(const MemoryReadStream&) = delete;
  MemoryReadStream& operator=(const MemoryReadStream&) = delete;

  ~MemoryReadStream() override;

  // ReadStream:
  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override;
  off_t Seek(off_t offset, int whence) override;

  base::span<const uint8_t> byte_buf() const { return byte_buf_; }

 protected:
  base::raw_span<const uint8_t> byte_buf_;
  off_t offset_;
};

// Reads the given |stream| until end-of-stream is reached. Returns (possibly
// empty) vector containing the data from the stream on success, or nullopt on
// error.
std::optional<std::vector<uint8_t>> ReadEntireStream(ReadStream& stream);

// CopyStreamToFile reads from `source` and writes the entire contents
// of it into `dest`. Returns true on success and false on failure.
bool CopyStreamToFile(ReadStream& source, base::File& dest);

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_
