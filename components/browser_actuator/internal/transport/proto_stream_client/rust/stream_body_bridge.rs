// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `cxx` FFI glue exposing the pure `stream_body_parser` crate to C++
//! (`RustStreamFramer`).
//!
//! This crate contains no parsing logic: it only converts between the
//! parser's idiomatic Rust types and cxx-compatible shared structs. It is
//! the only Rust crate in the proto stream client that needs
//! `allow_unsafe` (for the FFI machinery underpinning `cxx`); keep it that
//! way so the parser itself stays free of unsafe code.

chromium::import! {
    "//components/browser_actuator/internal/transport/proto_stream_client:stream_body_parser";
}

#[cxx::bridge(namespace = "browser_actuator::stream_bridge")]
mod ffi {
    /// One completed StreamBody `message` field: a serialized response
    /// proto for the C++ side to parse with protobuf-lite.
    struct StreamMessage {
        bytes: Vec<u8>,
    }

    /// Mirrors `stream_body_parser::FeedResult`. `status` is only
    /// meaningful when `has_status` is true (cxx shared structs cannot
    /// hold `Option`).
    struct StreamBodyFeedResult {
        messages: Vec<StreamMessage>,
        has_status: bool,
        status: Vec<u8>,
        noop_count: u64,
        failed: bool,
    }

    extern "Rust" {
        type StreamBodyParser;
        fn new_stream_body_parser() -> Box<StreamBodyParser>;
        fn feed(self: &mut StreamBodyParser, chunk: &[u8]) -> StreamBodyFeedResult;
    }
}

/// Opaque handle owning a parser instance; held as a `rust::Box` by the
/// C++ `RustStreamFramer`.
pub struct StreamBodyParser(stream_body_parser::Parser);

fn new_stream_body_parser() -> Box<StreamBodyParser> {
    Box::new(StreamBodyParser(stream_body_parser::Parser::new()))
}

impl StreamBodyParser {
    fn feed(&mut self, chunk: &[u8]) -> ffi::StreamBodyFeedResult {
        let result = self.0.feed(chunk);
        ffi::StreamBodyFeedResult {
            messages: result
                .messages
                .into_iter()
                .map(|bytes| ffi::StreamMessage { bytes })
                .collect(),
            has_status: result.status.is_some(),
            status: result.status.unwrap_or_default(),
            noop_count: result.noop_count,
            failed: result.failed,
        }
    }
}
