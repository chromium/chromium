// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/message_serialization.h"

#include "base/base64.h"
#include "chromecast/bindings/shared/proto_serializer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/bindings/cast_channel.pb.h"

using CastChannelMessage = cast::bindings::CastChannelMessage;

namespace cast_streaming {

const char kMirroringNamespace[] = "urn:x-cast:com.google.cast.webrtc";
const char kRemotingNamespace[] = "urn:x-cast:com.google.cast.remoting";
const char kSystemNamespace[] = "urn:x-cast:com.google.cast.system";
const char kInjectNamespace[] = "urn:x-cast:com.google.cast.inject";
const char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

const char kKeyData[] = "data";
const char kKeyType[] = "type";
const char kKeyRequestId[] = "requestId";
const char kKeyCode[] = "code";
const char kKeyStatus[] = "status";

const char kValueSystemSenderId[] = "SystemSender";
const char kValueWrapped[] = "WRAPPED";
const char kValueError[] = "ERROR";
const char kValueWrappedError[] = "WRAPPED_ERROR";
const char kValueMediaPlay[] = "PLAY";
const char kValueMediaPause[] = "PAUSE";
const char kValueMediaGetStatus[] = "GET_STATUS";
const char kValueMediaStatus[] = "MEDIA_STATUS";

const char kValueInjectNotSupportedError[] =
    R"({"code":"NOT_SUPPORTED","type":"ERROR"})";

const char kInitialConnectMessage[] = R"(
    {
      "type": "ready",
      "activeNamespaces": [
        "urn:x-cast:com.google.cast.webrtc",
        "urn:x-cast:com.google.cast.remoting",
        "urn:x-cast:com.google.cast.inject",
        "urn:x-cast:com.google.cast.media"
      ],
      "version": "2.0.0",
      "messagesVersion": "1.0"
    }
    )";

bool DeserializeCastMessage(base::StringPiece buffer,
                            std::string* sender_id,
                            std::string* message_namespace,
                            std::string* message) {
  absl::optional<CastChannelMessage> proto =
      chromecast::bindings::ProtoSerializer<CastChannelMessage>::Deserialize(
          buffer);
  if (!proto) {
    return false;
  }

  if (proto->payload_case() != CastChannelMessage::PayloadCase::kPayloadUtf8) {
    return false;
  }

  *sender_id = proto->sender_id();
  *message_namespace = proto->ns();
  *message = proto->payload_utf8();

  return true;
}

std::string SerializeCastMessage(const std::string& sender_id,
                                 const std::string& message_namespace,
                                 const std::string& message) {
  CastChannelMessage proto;
  proto.set_sender_id(sender_id);
  proto.set_ns(message_namespace);
  proto.set_payload_utf8(message);

  return chromecast::bindings::ProtoSerializer<CastChannelMessage>::Serialize(
      proto);
}

}  // namespace cast_streaming
