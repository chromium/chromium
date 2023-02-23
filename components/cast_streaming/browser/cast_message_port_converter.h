// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_CONVERTER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_CONVERTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace cast_api_bindings {
class MessagePort;
}

namespace openscreen::cast {
class MessagePort;
}

namespace cast_streaming {

// Responsible for creating a single openscreen MessagePort, wrapping any
// platform-specific implementation details.
class CastMessagePortConverter {
 public:
  static std::unique_ptr<CastMessagePortConverter> Create(
      ReceiverSession::MessagePortProvider message_port_provider,
      base::OnceClosure on_close);
  virtual ~CastMessagePortConverter() = default;

  // Gets the message port associated created from the |message_port_provider|
  // given to the ctor.
  virtual openscreen::cast::MessagePort& GetMessagePort() = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_CONVERTER_H_
