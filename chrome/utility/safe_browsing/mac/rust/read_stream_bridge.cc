// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/rust/read_stream_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "chrome/utility/safe_browsing/mac/rust/ffi.rs.h"

namespace safe_browsing::dmg::rust_ffi {

namespace {

class RustReadStreamAdapter : public ReadStream {
 public:
  explicit RustReadStreamAdapter(::rust::Box<RustReadStream> stream)
      : stream_(std::move(stream)) {}

  RustReadStreamAdapter(const RustReadStreamAdapter&) = delete;
  RustReadStreamAdapter& operator=(const RustReadStreamAdapter&) = delete;

  ~RustReadStreamAdapter() override = default;

  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override {
    CHECK(bytes_read);
    return stream_->read_stream(::rust::Slice<uint8_t>(buf), *bytes_read);
  }

  off_t Seek(off_t offset, int whence) override {
    int32_t posix_whence = 0;
    switch (whence) {
      case SEEK_SET:
        posix_whence = 0;
        break;
      case SEEK_CUR:
        posix_whence = 1;
        break;
      case SEEK_END:
        posix_whence = 2;
        break;
      default:
        return -1;
    }
    return static_cast<off_t>(stream_->seek_stream(offset, posix_whence));
  }

 private:
  ::rust::Box<RustReadStream> stream_;
};

}  // namespace

std::unique_ptr<ReadStream> WrapRustReadStream(
    ::rust::Box<RustReadStream> rust_stream) {
  if (!rust_stream->is_valid()) {
    return nullptr;
  }
  return std::make_unique<RustReadStreamAdapter>(std::move(rust_stream));
}

}  // namespace safe_browsing::dmg::rust_ffi
