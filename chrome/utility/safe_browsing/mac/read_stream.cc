// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/read_stream.h"

#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <array>

#include "base/check.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"

namespace safe_browsing {
namespace dmg {

bool ReadStream::ReadExact(base::span<uint8_t> buffer) {
  size_t bytes_read = 0;
  return Read(buffer, &bytes_read) && bytes_read == buffer.size();
}

FileReadStream::FileReadStream(int fd) : fd_(fd) {}

FileReadStream::~FileReadStream() {}

bool FileReadStream::Read(base::span<uint8_t> buf, size_t* bytes_read) {
  *bytes_read = 0;
  ssize_t signed_bytes_read = HANDLE_EINTR(read(fd_, buf.data(), buf.size()));
  if (signed_bytes_read < 0)
    return false;
  *bytes_read = signed_bytes_read;
  return true;
}

off_t FileReadStream::Seek(off_t offset, int whence) {
  return lseek(fd_, offset, whence);
}

MemoryReadStream::MemoryReadStream(base::span<const uint8_t> byte_buf)
    : byte_buf_(byte_buf), offset_(0) {}

MemoryReadStream::~MemoryReadStream() {}

bool MemoryReadStream::Read(base::span<uint8_t> buf, size_t* bytes_read) {
  *bytes_read = 0;

  size_t bytes_remaining = byte_buf_.size() - offset_;
  if (bytes_remaining == 0) {
    return true;
  }

  *bytes_read = std::min(buf.size(), bytes_remaining);
  buf.first(*bytes_read).copy_from(byte_buf_.subspan(offset_, *bytes_read));
  offset_ += *bytes_read;
  return true;
}

off_t MemoryReadStream::Seek(off_t offset, int whence) {
  switch (whence) {
    case SEEK_SET:
      offset_ = offset;
      break;
    case SEEK_CUR:
      offset_ += offset;
      break;
    case SEEK_END:
      offset_ = byte_buf_.size() + offset;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return -1;
  }
  if (static_cast<size_t>(offset_) >= byte_buf_.size()) {
    offset_ = byte_buf_.size();
  }
  return offset_;
}

std::optional<std::vector<uint8_t>> ReadEntireStream(ReadStream& stream) {
  std::vector<uint8_t> data;
  std::array<uint8_t, 1024> buffer;
  size_t bytes_read = 0;
  do {
    if (!stream.Read(buffer, &bytes_read)) {
      return std::nullopt;
    }

    data.insert(data.end(), buffer.begin(), buffer.begin() + bytes_read);
  } while (bytes_read != 0);

  return data;
}

bool CopyStreamToFile(ReadStream& source, base::File& dest) {
  dest.Seek(base::File::Whence::FROM_BEGIN, 0);
  std::array<uint8_t, 1024> buffer;
  size_t bytes_read = 0;
  do {
    if (!source.Read(buffer, &bytes_read)) {
      return false;
    }
    if (!dest.WriteAtCurrentPosAndCheck(base::span(buffer).first(bytes_read))) {
      return false;
    }
  } while (bytes_read > 0);
  return true;
}

}  // namespace dmg
}  // namespace safe_browsing
