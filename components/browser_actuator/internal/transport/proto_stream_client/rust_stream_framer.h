// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_RUST_STREAM_FRAMER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_RUST_STREAM_FRAMER_H_

#include <memory>
#include <string_view>

#include "components/browser_actuator/internal/transport/stream_framer.h"

namespace browser_actuator {

// StreamFramer for OnePlatform `alt=proto` StreamBody framing, backed by
// the pure-Rust `stream_body_parser` crate: each complete top-level
// length-delimited field of the never-ending StreamBody proto is one
// delivery (message, terminal status, or discarded keep-alive noop).
//
// A deliberate encapsulation trade: behind the pimpl, the cxx-generated
// bridge header (and cxx.h itself) are included by exactly one
// production .cc, keeping every FFI touchpoint reviewable in one place.
// Holding the rust::Box directly (cxx.h plus a forward-declared opaque
// type) would also work and save an indirection, at the cost of
// spreading rust:: types to every includer; one extra pointer on a
// per-connection object is the cheaper side of that trade.
class RustStreamFramer : public StreamFramer {
 public:
  RustStreamFramer();
  RustStreamFramer(const RustStreamFramer&) = delete;
  RustStreamFramer& operator=(const RustStreamFramer&) = delete;
  ~RustStreamFramer() override;

  // Returns a factory minting a fresh RustStreamFramer per call.
  static StreamFramerFactory MakeFactory();

  // StreamFramer:
  FeedResult Feed(std::string_view chunk) override;

 private:
  // Hides the rust::Box holding the parser.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_RUST_STREAM_FRAMER_H_
