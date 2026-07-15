// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_FRAMER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_FRAMER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"

namespace browser_actuator {

// Splits an untrusted byte stream into the complete messages it frames.
// This is the seam between a MessageStreamClient implementation, which
// owns the connection and receives raw body chunks, and the framing
// parser, which is the only code that interprets those bytes (see
// //docs/security/rule-of-2.md — the production framer is pure Rust).
//
// A framer is single-use: it holds the framing state of one connection
// and is discarded with it. Anything buffered mid-field when the stream
// ends was never completed and needs no explicit end-of-stream call.
class StreamFramer {
 public:
  // The outcome of feeding one chunk of bytes to the framer.
  //
  // Within a single feed, the relative order of `messages` and `status`
  // is not preserved (`status` is a single slot); per the wire contract
  // the status is the final field of the RPC, after all messages.
  struct FeedResult {
    // Payloads of completed messages, in stream order: each is one
    // serialized response proto for the layer above to parse.
    std::vector<std::string> messages;

    // Payload of a completed terminal status (a serialized
    // google.rpc.Status): the RPC is over. Note that an engaged, empty
    // value is meaningful — a google.rpc.Status with code OK serializes
    // to zero bytes.
    std::optional<std::string> status;

    // True if the stream is malformed or a hardening limit was exceeded.
    // Terminal: all further input is ignored, and the owner should drop
    // the connection.
    bool failed = false;
  };

  virtual ~StreamFramer() = default;

  // Frames one chunk of the stream, returning anything completed by it.
  // Chunk boundaries need not align with field boundaries.
  virtual FeedResult Feed(std::string_view chunk) = 0;
};

// Mints a fresh framer. MessageStreamClient implementations take a
// factory rather than a framer because they need a new one per
// connection: framing state must not survive a reconnect.
using StreamFramerFactory =
    base::RepeatingCallback<std::unique_ptr<StreamFramer>()>;

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_FRAMER_H_
