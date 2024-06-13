// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <vector>

#include "base/files/file.h"

namespace safe_browsing {
namespace dmg {

// An interface for reading and seeking over a byte stream.
class ReadStream {
 public:
  virtual ~ReadStream() {}

  // Copies up to |size| bytes into |data|. Returns |true| on success with the
  // actual number of bytes written stored in |size|. Returns |false| on error.
  // At end-of-stream, returns |true| with |bytes_read| set to 0.
  virtual bool Read(uint8_t* data, size_t size, size_t* bytes_read) = 0;

  // Calls |Read| but only returns true if the number of bytes read equals
  // |size|.
  bool ReadExact(uint8_t* data, size_t size);

  template <typename T>
  bool ReadType(T* t) {
    return ReadExact(reinterpret_cast<uint8_t*>(t), sizeof(T));
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
  bool Read(uint8_t* data, size_t size, size_t* bytes_read) override;
  off_t Seek(off_t offset, int whence) override;

 private:
  int fd_;
};

// An implementation of ReadStream that operates on a byte buffer. This class
// does not take ownership of the underlying byte array.
class MemoryReadStream : public ReadStream {
 public:
  MemoryReadStream(const uint8_t* data, size_t size);

  MemoryReadStream(const MemoryReadStream&) = delete;
  MemoryReadStream& operator=(const MemoryReadStream&) = delete;

  ~MemoryReadStream() override;

  // ReadStream:
  bool Read(uint8_t* data, size_t size, size_t* bytes_read) override;
  off_t Seek(off_t offset, int whence) override;

  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }

 protected:
  const uint8_t* data_;
  size_t size_;
  off_t offset_;
};

// Reads the given |stream| until end-of-stream is reached, storying the read
// bytes into |data|. Returns true on success and false on error.
bool ReadEntireStream(ReadStream* stream, std::vector<uint8_t>* data);

// CopyStreamToFile reads from `source` and writes the entire contents
// of it into `dest`. Returns true on success and false on failure.
bool CopyStreamToFile(ReadStream* source, base::File& dest);

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_READ_STREAM_H_
