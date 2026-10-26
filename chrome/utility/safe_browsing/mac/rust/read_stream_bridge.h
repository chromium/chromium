// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <memory>

#include "base/check.h"
#include "base/containers/span.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace safe_browsing::dmg::rust_ffi {

using ReadStream = ::safe_browsing::dmg::ReadStream;
struct RustReadStream;

// Called by Rust (`rust/ffi.rs`) to read/seek a C++ `ReadStream`. Defined
// inline so any target linking `dmg_ffi_bridge` (including targets in
// `//chrome/common` that do not link `dmg_common`) resolves the `cxx`
// `extern "C++"` bridge symbols in debug builds.
inline bool RustBridgeRead(ReadStream* stream,
                           ::rust::Slice<uint8_t> buf,
                           size_t& bytes_read) {
  CHECK(stream);
  return stream->Read(base::span<uint8_t>(buf), &bytes_read);
}

inline int64_t RustBridgeSeek(ReadStream* stream,
                              int64_t offset,
                              int32_t whence) {
  CHECK(stream);
  int posix_whence = SEEK_SET;
  switch (whence) {
    case 0:
      posix_whence = SEEK_SET;
      break;
    case 1:
      posix_whence = SEEK_CUR;
      break;
    case 2:
      posix_whence = SEEK_END;
      break;
    default:
      return -1;
  }
  return stream->Seek(static_cast<off_t>(offset), posix_whence);
}

// Takes ownership of a boxed `RustReadStream` and wraps it into a C++
// `safe_browsing::dmg::ReadStream`.
std::unique_ptr<ReadStream> WrapRustReadStream(
    ::rust::Box<RustReadStream> rust_stream);

}  // namespace safe_browsing::dmg::rust_ffi

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_
