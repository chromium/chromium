// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "third_party/rust/cxx/v1/cxx.h"

namespace safe_browsing::dmg {
class ReadStream;
}  // namespace safe_browsing::dmg

namespace safe_browsing::dmg::rust_ffi {

using ReadStream = ::safe_browsing::dmg::ReadStream;
struct RustReadStream;

// Called by Rust (`rust/ffi.rs`) to read/seek a C++ `ReadStream`.
bool RustBridgeRead(ReadStream* stream,
                    ::rust::Slice<uint8_t> buf,
                    size_t& bytes_read);
int64_t RustBridgeSeek(ReadStream* stream, int64_t offset, int32_t whence);

// Takes ownership of a boxed `RustReadStream` and wraps it into a C++
// `safe_browsing::dmg::ReadStream`.
std::unique_ptr<ReadStream> WrapRustReadStream(
    ::rust::Box<RustReadStream> rust_stream);

}  // namespace safe_browsing::dmg::rust_ffi

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_RUST_READ_STREAM_BRIDGE_H_
