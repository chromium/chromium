// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Incremental parser for OnePlatform / ESF protobuf stream framing.
//!
//! When a OnePlatform endpoint is asked for a server-streaming RPC with
//! `alt=proto` (`Accept: application/x-protobuf`), the response body is one
//! never-ending serialized proto of the shape:
//!
//! ```proto
//! message StreamBody {
//!   repeated bytes message = 1;      // One per streamed response message.
//!   google.rpc.Status status = 2;    // At most once, at the end of the RPC.
//!   repeated bytes noop = 15;        // Keep-alive padding; no meaning.
//! }
//! ```
//!
//! Because protobuf messages are a flat sequence of tag/value pairs
//! (<https://protobuf.dev/programming-guides/encoding/>), the stream can be
//! parsed one top-level field at a time: each complete length-delimited
//! field is one delivery. There is no other chunking layer.
//!
//! This crate is deliberately pure: no dependencies, no I/O, no `unsafe`.
//! Bytes go in via [`Parser::feed`]; framed payloads come out. The payloads
//! themselves are parsed by the C++ caller using protobuf-lite; this crate
//! only makes the framing decisions about untrusted input.
//!
//! # Security
//!
//! In addition to Rust's memory safety, the parser enforces
//! [`MAX_MESSAGE_BYTES`] on every length-delimited field (known or unknown)
//! and rejects malformed varints, invalid field numbers, and the deprecated
//! group wire types. Any violation puts the parser into a terminal failed
//! state (reported via [`FeedResult::failed`]); the owner is expected to
//! drop the connection.

#![forbid(unsafe_code)]

/// Maximum size of a single length-delimited field, applied to unknown
/// fields as well (a skipped field still has to be buffered past).
pub const MAX_MESSAGE_BYTES: usize = 10 * 1024 * 1024;

/// StreamBody field numbers.
const FIELD_MESSAGE: u32 = 1;
const FIELD_STATUS: u32 = 2;
const FIELD_NOOP: u32 = 15;

/// Protobuf wire types.
const WIRETYPE_VARINT: u32 = 0;
const WIRETYPE_I64: u32 = 1;
const WIRETYPE_LEN: u32 = 2;
const WIRETYPE_SGROUP: u32 = 3;
const WIRETYPE_EGROUP: u32 = 4;
const WIRETYPE_I32: u32 = 5;

/// The outcome of feeding one chunk of bytes to the parser.
///
/// Within a single feed, the relative order of `messages` and `status` is
/// not preserved (`status` is a single slot); per the StreamBody contract
/// the status is the final field of the RPC, after all messages.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct FeedResult {
    /// Payloads of completed `message` fields, in stream order. Each is one
    /// serialized response proto for the C++ side to parse.
    pub messages: Vec<Vec<u8>>,
    /// Payload of a completed `status` field (a serialized
    /// `google.rpc.Status`): the RPC is over. A repeated status overwrites
    /// this slot (last one wins), per protobuf merge semantics.
    pub status: Option<Vec<u8>>,
    /// Number of completed `noop` (keep-alive) fields; their content is
    /// meaningless and discarded.
    pub noop_count: u64,
    /// True if the stream is malformed or a hardening limit was exceeded.
    /// Terminal: all further input is ignored, and the owner should drop
    /// the connection.
    pub failed: bool,
}

/// Result of one step of parsing at some buffer position.
enum Step {
    /// A complete field was consumed; parsing continues at this position.
    Consumed(usize),
    /// The buffer ends mid-field; wait for more input.
    NeedMore,
    /// The stream is not valid StreamBody framing.
    Malformed,
}

/// Incremental StreamBody parser. Feed it chunks of bytes as they arrive
/// from the network; field boundaries need not align with chunk boundaries.
///
/// End-of-stream requires no explicit call: a field pending at EOF was
/// never completed and is discarded with the parser.
#[derive(Default)]
pub struct Parser {
    /// Bytes received but not yet consumed as complete fields.
    ///
    /// Memory bound: an over-limit length claim is rejected before its
    /// payload is ever buffered, so what is live here is at most one
    /// incomplete field (just under [`MAX_MESSAGE_BYTES`]) plus the chunk
    /// being fed. Note that `Vec` keeps its capacity when drained (and
    /// amortized growth may briefly overshoot), so the allocation stays at
    /// its high-water mark until the parser fails or is dropped.
    pending: Vec<u8>,
    failed: bool,
}

impl Parser {
    pub fn new() -> Parser {
        Parser::default()
    }

    /// Parses one chunk of the stream, returning any fields completed by it.
    ///
    /// `chunk` is buffered without a size check: its size is chosen by the
    /// embedder's own network stack (bounded by its read buffer), not by
    /// the remote end. What the remote end does control — how much data a
    /// field claims to need before it completes — is what
    /// [`MAX_MESSAGE_BYTES`] bounds.
    pub fn feed(&mut self, chunk: &[u8]) -> FeedResult {
        let mut result = FeedResult::default();
        if !self.failed {
            self.pending.extend_from_slice(chunk);
            let mut pos = 0;
            loop {
                match parse_field(&self.pending[pos..], &mut result) {
                    Step::Consumed(len) => pos += len,
                    Step::NeedMore => break,
                    Step::Malformed => {
                        self.failed = true;
                        break;
                    }
                }
            }
            // Compact the buffer once per feed, not once per field:
            // drain(..pos) frees the consumed prefix and memmoves the
            // unconsumed tail (at most one incomplete field) to the front.
            // Draining inside the loop would make a feed that completes N
            // fields quadratic in the buffered bytes.
            self.pending.drain(..pos);
        }
        if self.failed {
            // The parser will never make progress again; free the buffer.
            self.pending = Vec::new();
            result.failed = true;
        }
        result
    }
}

/// Parses one top-level field at the start of `buf`. `Consumed` carries
/// the field's total encoded length.
fn parse_field(buf: &[u8], result: &mut FeedResult) -> Step {
    let (tag, after_tag) = match read_varint(buf) {
        VarintStep::Value(tag, len) => (tag, len),
        VarintStep::NeedMore => return Step::NeedMore,
        VarintStep::Malformed => return Step::Malformed,
    };
    // Tags must fit in 32 bits per the protobuf spec, but the wire can
    // encode up to 64; validate while still in u64 so that the `as u32`
    // narrowing casts below are lossless.
    if tag > u64::from(u32::MAX) {
        return Step::Malformed;
    }
    // A tag packs `field_number << 3 | wire_type`: the low 3 bits are the
    // wire type (masked out with 0x7), and everything above them is the
    // field number.
    let field_number = (tag >> 3) as u32;
    let wire_type = (tag & 0x7) as u32;
    // Field number 0 is reserved and never valid.
    if field_number == 0 {
        return Step::Malformed;
    }

    match wire_type {
        // Unknown scalar fields are skipped, tolerating schema evolution.
        WIRETYPE_VARINT => match read_varint(&buf[after_tag..]) {
            VarintStep::Value(_, len) => Step::Consumed(after_tag + len),
            VarintStep::NeedMore => Step::NeedMore,
            VarintStep::Malformed => Step::Malformed,
        },
        WIRETYPE_I64 => skip_fixed(buf, after_tag, 8),
        WIRETYPE_I32 => skip_fixed(buf, after_tag, 4),
        WIRETYPE_LEN => {
            let (length, after_length) = match read_varint(&buf[after_tag..]) {
                VarintStep::Value(length, len) => (length, after_tag + len),
                VarintStep::NeedMore => return Step::NeedMore,
                VarintStep::Malformed => return Step::Malformed,
            };
            if length > MAX_MESSAGE_BYTES as u64 {
                return Step::Malformed;
            }
            // Deliberate shadow: from here on, `length` is the usize that
            // passed the size check, and the unvalidated u64 can no longer
            // be reached by accident.
            let length = length as usize;
            // Phrased as a subtraction that cannot underflow — the length
            // varint came out of `buf`, so `after_length <= buf.len()`.
            // The addition form `after_length + length > buf.len()` could
            // overflow instead.
            if buf.len() - after_length < length {
                return Step::NeedMore;
            }
            let payload = &buf[after_length..after_length + length];
            match field_number {
                FIELD_MESSAGE => result.messages.push(payload.to_vec()),
                // A repeated status deliberately overwrites (last one
                // wins): those are protobuf merge semantics for singular
                // fields, and they are what make concatenating two
                // serialized messages equivalent to merging them, and
                // singular <-> repeated field changes wire-compatible.
                // The framer must not be stricter than the format.
                FIELD_STATUS => result.status = Some(payload.to_vec()),
                FIELD_NOOP => result.noop_count += 1,
                // Unknown length-delimited fields are skipped.
                _ => {}
            }
            Step::Consumed(after_length + length)
        }
        // Groups are deprecated and cannot legitimately appear here.
        WIRETYPE_SGROUP | WIRETYPE_EGROUP => Step::Malformed,
        // Wire types 6 and 7 do not exist.
        _ => Step::Malformed,
    }
}

/// Skips a fixed-width scalar (I64 is 8 bytes, I32 is 4): either every
/// byte of the value is already buffered and the field is consumed, or we
/// wait for more input. Same underflow-safe comparison shape as the LEN
/// arm of `parse_field`.
fn skip_fixed(buf: &[u8], pos: usize, width: usize) -> Step {
    if buf.len() - pos < width {
        Step::NeedMore
    } else {
        Step::Consumed(pos + width)
    }
}

enum VarintStep {
    Value(u64, usize),
    NeedMore,
    Malformed,
}

/// Reads a base-128 varint at the start of `buf`. `Value` carries the
/// decoded value and its encoded length.
///
/// Varint encoding: each byte carries 7 payload bits, least-significant
/// group first, and the top bit of each byte is a continuation flag — 1
/// means "more bytes follow". A u64 therefore needs at most 10 bytes:
/// nine bytes carry 9 * 7 = 63 bits, and a 10th byte may only contribute
/// the one remaining top bit.
fn read_varint(buf: &[u8]) -> VarintStep {
    let mut value: u64 = 0;
    // Running exactly 10 iterations is itself a hardening bound: an
    // attacker sending endless continuation bytes falls out of the loop
    // into Malformed below instead of being followed forever.
    for i in 0..10 {
        // Bounds check and read in one step: `get` returns None past the
        // end of the buffer, which is not an error here — the rest of the
        // varint simply has not arrived yet.
        let Some(&byte) = buf.get(i) else {
            return VarintStep::NeedMore;
        };
        // Byte 10 (i == 9) is special-cased: only bit 63 of the value is
        // still unfilled.
        if i == 9 {
            // Its continuation flag (0x80) must be clear — nothing may
            // follow — and of its payload bits only the lowest may be
            // set; 0x7E catches the six that would overflow a u64. The
            // legal 10th bytes are exactly 0x00 and 0x01.
            if byte & 0x7E != 0 || byte & 0x80 != 0 {
                return VarintStep::Malformed;
            }
            value |= u64::from(byte & 0x01) << 63;
            return VarintStep::Value(value, i + 1);
        }
        // Keep the 7 payload bits (0x7F strips the continuation flag) and
        // shift them into place: group i holds value bits 7*i..=7*i+6.
        value |= u64::from(byte & 0x7F) << (i * 7);
        // Continuation flag clear: this was the final byte of the varint.
        if byte & 0x80 == 0 {
            return VarintStep::Value(value, i + 1);
        }
    }
    // Ten continuation flags in a row: no valid u64 varint does that.
    VarintStep::Malformed
}
