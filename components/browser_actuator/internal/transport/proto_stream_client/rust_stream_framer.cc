// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/proto_stream_client/rust_stream_framer.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/rust/stream_body_bridge.rs.h"

namespace browser_actuator {

struct RustStreamFramer::Impl {
  rust::Box<stream_bridge::StreamBodyParser> parser =
      stream_bridge::new_stream_body_parser();
};

RustStreamFramer::RustStreamFramer() : impl_(std::make_unique<Impl>()) {}

RustStreamFramer::~RustStreamFramer() = default;

// static
StreamFramerFactory RustStreamFramer::MakeFactory() {
  return base::BindRepeating([]() -> std::unique_ptr<StreamFramer> {
    return std::make_unique<RustStreamFramer>();
  });
}

StreamFramer::FeedResult RustStreamFramer::Feed(std::string_view chunk) {
  const base::span<const uint8_t> bytes = base::as_byte_span(chunk);
  stream_bridge::StreamBodyFeedResult raw =
      impl_->parser->feed(rust::Slice(bytes));

  FeedResult result;
  result.messages.reserve(raw.messages.size());
  for (const stream_bridge::StreamMessage& message : raw.messages) {
    result.messages.emplace_back(message.bytes.begin(), message.bytes.end());
  }
  if (raw.has_status) {
    result.status = std::string(raw.status.begin(), raw.status.end());
  }
  // The bridge's StreamBodyFeedResult carries one more field, `noop_count`
  // (how many keep-alive fields the parser consumed). Dropping it here is
  // deliberate, not an oversight: keep-alives exist only to prove the
  // connection is alive, clients observe liveness from the raw received
  // bytes before framing, and so StreamFramer::FeedResult has no use for
  // a count.
  result.failed = raw.failed;
  return result;
}

}  // namespace browser_actuator
