// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/route_message_util.h"

#include "base/json/json_writer.h"
#include "base/values.h"

using media_router::mojom::RouteMessage;
using media_router::mojom::RouteMessagePtr;

namespace media_router {
namespace message_util {

media_router::mojom::RouteMessagePtr RouteMessageFromValue(
    base::Value message) {
  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return RouteMessageFromString(std::move(str));
}

RouteMessagePtr RouteMessageFromString(std::string message) {
  auto route_message = RouteMessage::New();
  route_message->type = RouteMessage::Type::TEXT;
  route_message->message = std::move(message);
  return route_message;
}

RouteMessagePtr RouteMessageFromData(std::vector<uint8_t> data) {
  auto route_message = RouteMessage::New();
  route_message->type = RouteMessage::Type::BINARY;
  route_message->data = std::move(data);
  return route_message;
}

blink::mojom::PresentationConnectionMessagePtr
PresentationConnectionFromRouteMessage(RouteMessagePtr route_message) {
  // NOTE: Makes a copy of |route_message| contents.  This can be eliminated
  // when media_router::mojom::RouteMessage is deleted.
  switch (route_message->type) {
    case RouteMessage::Type::TEXT:
      return blink::mojom::PresentationConnectionMessage::NewMessage(
          route_message->message.value());
    case RouteMessage::Type::BINARY:
      return blink::mojom::PresentationConnectionMessage::NewData(
          route_message->data.value());
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown RouteMessageType " << route_message->type;
  return nullptr;
}

}  // namespace message_util
}  // namespace media_router
