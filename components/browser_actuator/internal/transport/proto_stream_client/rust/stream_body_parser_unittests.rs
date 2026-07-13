// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//components/browser_actuator/internal/transport/proto_stream_client:stream_body_parser";
}

use rust_gtest_interop::prelude::*;
use stream_body_parser::{FeedResult, Parser, MAX_MESSAGE_BYTES};

/// Encodes a varint.
fn varint(mut value: u64) -> Vec<u8> {
    let mut out = Vec::new();
    loop {
        let byte = (value & 0x7F) as u8;
        value >>= 7;
        if value == 0 {
            out.push(byte);
            return out;
        }
        out.push(byte | 0x80);
    }
}

/// Encodes one length-delimited field.
fn len_field(field_number: u32, payload: &[u8]) -> Vec<u8> {
    let mut out = varint(u64::from(field_number << 3 | 2));
    out.extend_from_slice(&varint(payload.len() as u64));
    out.extend_from_slice(payload);
    out
}

fn feed_all(input: &[u8]) -> FeedResult {
    Parser::new().feed(input)
}

#[gtest(StreamBodyParserRustTest, SingleMessage)]
fn single_message() {
    let result = feed_all(&len_field(1, b"hello proto"));
    expect_eq!(result.messages, vec![b"hello proto".to_vec()]);
    expect_eq!(result.status, None);
    expect_eq!(result.noop_count, 0);
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, MultipleMessagesInOneChunk)]
fn multiple_messages_in_one_chunk() {
    let mut input = len_field(1, b"one");
    input.extend_from_slice(&len_field(1, b"two"));
    input.extend_from_slice(&len_field(1, b"three"));
    let result = feed_all(&input);
    expect_eq!(result.messages, vec![b"one".to_vec(), b"two".to_vec(), b"three".to_vec()]);
}

#[gtest(StreamBodyParserRustTest, EmptyMessageIsValid)]
fn empty_message_is_valid() {
    let result = feed_all(&len_field(1, b""));
    expect_eq!(result.messages, vec![Vec::<u8>::new()]);
}

#[gtest(StreamBodyParserRustTest, StatusDelivered)]
fn status_delivered() {
    let mut input = len_field(1, b"last message");
    input.extend_from_slice(&len_field(2, b"serialized rpc status"));
    let result = feed_all(&input);
    expect_eq!(result.messages, vec![b"last message".to_vec()]);
    expect_eq!(result.status, Some(b"serialized rpc status".to_vec()));
}

#[gtest(StreamBodyParserRustTest, DuplicateStatusLastWins)]
fn duplicate_status_last_wins() {
    // Protobuf merge semantics for a singular field: the last value on
    // the wire wins. This is deliberate leniency — it is what makes
    // concatenating two serialized messages equivalent to merging them,
    // and singular <-> repeated field changes wire-compatible — so the
    // framer must not reject it.
    let mut input = len_field(2, b"first status");
    input.extend_from_slice(&len_field(2, b"second status"));
    let result = feed_all(&input);
    expect_eq!(result.status, Some(b"second status".to_vec()));
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, DuplicateStatusAcrossChunksLastWins)]
fn duplicate_status_across_chunks_last_wins() {
    // Across feeds, each FeedResult reports the status it completed; the
    // consumer's view is always the most recent one.
    let mut parser = Parser::new();
    let first = parser.feed(&len_field(2, b"first"));
    expect_eq!(first.status, Some(b"first".to_vec()));
    expect_false!(first.failed);
    let second = parser.feed(&len_field(2, b"second"));
    expect_eq!(second.status, Some(b"second".to_vec()));
    expect_false!(second.failed);
}

#[gtest(StreamBodyParserRustTest, FieldsAfterStatusStillParse)]
fn fields_after_status_still_parse() {
    // The contract says the status is the RPC's final field, but trailing
    // messages or noops are a server quirk, not a framing hazard; the
    // client stops at the stream close that follows anyway.
    let mut input = len_field(2, b"status");
    input.extend_from_slice(&len_field(1, b"late message"));
    input.extend_from_slice(&len_field(15, b"noop"));
    let result = feed_all(&input);
    expect_eq!(result.status, Some(b"status".to_vec()));
    expect_eq!(result.messages, vec![b"late message".to_vec()]);
    expect_eq!(result.noop_count, 1);
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, NoopCountedNotDelivered)]
fn noop_counted_not_delivered() {
    let mut input = len_field(15, b"padding padding");
    input.extend_from_slice(&len_field(15, b""));
    input.extend_from_slice(&len_field(1, b"real"));
    let result = feed_all(&input);
    expect_eq!(result.noop_count, 2);
    expect_eq!(result.messages, vec![b"real".to_vec()]);
}

#[gtest(StreamBodyParserRustTest, MessageSplitAcrossChunks)]
fn message_split_across_chunks() {
    let input = len_field(1, b"split across many chunks");
    let mut parser = Parser::new();
    let mut messages = Vec::new();
    for byte in &input {
        let result = parser.feed(&[*byte]);
        expect_false!(result.failed);
        messages.extend(result.messages);
    }
    expect_eq!(messages, vec![b"split across many chunks".to_vec()]);
}

#[gtest(StreamBodyParserRustTest, MultiByteLengthVarintSplitAcrossChunks)]
fn multi_byte_length_varint_split_across_chunks() {
    // A 300-byte payload needs a two-byte length varint; split inside it.
    let payload = vec![0xAB; 300];
    let input = len_field(1, &payload);
    let mut parser = Parser::new();
    expect_true!(parser.feed(&input[..2]).messages.is_empty());
    let result = parser.feed(&input[2..]);
    expect_eq!(result.messages, vec![payload]);
}

#[gtest(StreamBodyParserRustTest, LiveEndpointShape)]
fn live_endpoint_shape() {
    // The framing observed from a real OnePlatform endpoint: a 0x0A tag,
    // one-byte length, serialized CountResponse payload, repeated.
    let count_response = b"\x08\x06\x12\x0b\x0a\x09message 3\x1a\x013";
    let mut input = len_field(1, count_response);
    input.extend_from_slice(&len_field(1, count_response));
    let result = feed_all(&input);
    expect_eq!(result.messages.len(), 2);
    expect_eq!(result.messages[0], count_response.to_vec());
}

#[gtest(StreamBodyParserRustTest, UnknownScalarFieldsSkipped)]
fn unknown_scalar_fields_skipped() {
    // Field 3 varint, field 4 fixed64, field 5 fixed32, then a message.
    let mut input = vec![0x18];
    input.extend_from_slice(&varint(123456789));
    input.push(0x21);
    input.extend_from_slice(&[0; 8]);
    input.push(0x2D);
    input.extend_from_slice(&[0; 4]);
    input.extend_from_slice(&len_field(1, b"after unknowns"));
    let result = feed_all(&input);
    expect_eq!(result.messages, vec![b"after unknowns".to_vec()]);
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, UnknownLenFieldSkipped)]
fn unknown_len_field_skipped() {
    let mut input = len_field(9, b"future StreamBody field");
    input.extend_from_slice(&len_field(1, b"still parsed"));
    let result = feed_all(&input);
    expect_eq!(result.messages, vec![b"still parsed".to_vec()]);
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, UnknownFixedFieldsSplitAcrossChunks)]
fn unknown_fixed_fields_split_across_chunks() {
    let mut parser = Parser::new();
    // Field 4, fixed64: tag arrives, then the 8 bytes trickle in.
    expect_false!(parser.feed(&[0x21, 0x01, 0x02]).failed);
    expect_false!(parser.feed(&[0x03, 0x04, 0x05, 0x06, 0x07]).failed);
    let result = parser.feed(&[0x08]);
    expect_false!(result.failed);
    // The parser is aligned again: a normal message parses.
    let result = parser.feed(&len_field(1, b"aligned"));
    expect_eq!(result.messages, vec![b"aligned".to_vec()]);
}

#[gtest(StreamBodyParserRustTest, GroupWireTypesFail)]
fn group_wire_types_fail() {
    // Field 1 with SGROUP wire type (3): tag 0x0B.
    expect_true!(feed_all(&[0x0B]).failed);
    // Field 1 with EGROUP wire type (4): tag 0x0C.
    expect_true!(feed_all(&[0x0C]).failed);
}

#[gtest(StreamBodyParserRustTest, InvalidWireTypesFail)]
fn invalid_wire_types_fail() {
    // Wire types 6 and 7 do not exist.
    expect_true!(feed_all(&[0x0E]).failed);
    expect_true!(feed_all(&[0x0F]).failed);
}

#[gtest(StreamBodyParserRustTest, FieldNumberZeroFails)]
fn field_number_zero_fails() {
    // Tag 0x02 = field 0, LEN.
    expect_true!(feed_all(&[0x02, 0x00]).failed);
}

#[gtest(StreamBodyParserRustTest, OversizedMessageFails)]
fn oversized_message_fails() {
    let mut input = varint(1 << 3 | 2);
    input.extend_from_slice(&varint((MAX_MESSAGE_BYTES + 1) as u64));
    let result = feed_all(&input);
    expect_true!(result.failed);
}

#[gtest(StreamBodyParserRustTest, OversizedUnknownLenFieldFails)]
fn oversized_unknown_len_field_fails() {
    // A skipped field must still respect the limit, or it could force
    // unbounded buffering.
    let mut input = varint(9 << 3 | 2);
    input.extend_from_slice(&varint(u64::MAX));
    expect_true!(feed_all(&input).failed);
}

#[gtest(StreamBodyParserRustTest, MessageAtSizeLimitParses)]
fn message_at_size_limit_parses() {
    let payload = vec![0x42; MAX_MESSAGE_BYTES];
    let result = feed_all(&len_field(1, &payload));
    expect_eq!(result.messages.len(), 1);
    expect_eq!(result.messages[0].len(), MAX_MESSAGE_BYTES);
    expect_false!(result.failed);
}

#[gtest(StreamBodyParserRustTest, OverlongVarintFails)]
fn overlong_varint_fails() {
    // Eleven continuation bytes can never be a valid varint.
    expect_true!(feed_all(&[0x80; 11]).failed);
}

#[gtest(StreamBodyParserRustTest, TenByteVarintWithExcessBitsFails)]
fn ten_byte_varint_with_excess_bits_fails() {
    // The 10th byte may only contribute the top bit of a u64.
    let mut input = vec![0xFF; 9];
    input.push(0x02);
    expect_true!(feed_all(&input).failed);
}

#[gtest(StreamBodyParserRustTest, FailureIsTerminal)]
fn failure_is_terminal() {
    let mut parser = Parser::new();
    expect_true!(parser.feed(&[0x0B]).failed);
    let result = parser.feed(&len_field(1, b"well-formed"));
    expect_true!(result.failed);
    expect_true!(result.messages.is_empty());
}

#[gtest(StreamBodyParserRustTest, IncompleteFieldDiscardedAtEof)]
fn incomplete_field_discarded_at_eof() {
    // Never completed: no delivery. Dropping the parser is all the EOF
    // handling there is.
    let input = len_field(1, b"never finished");
    let mut parser = Parser::new();
    let result = parser.feed(&input[..input.len() - 1]);
    expect_true!(result.messages.is_empty());
    expect_false!(result.failed);
}
