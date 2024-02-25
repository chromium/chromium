// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/common/message_serialization.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace cast_streaming {

const char kMirroringNamespace[] = "urn:x-cast:com.google.cast.webrtc";
const char kRemotingNamespace[] = "urn:x-cast:com.google.cast.remoting";
const char kSystemNamespace[] = "urn:x-cast:com.google.cast.system";
const char kInjectNamespace[] = "urn:x-cast:com.google.cast.inject";
const char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

const char kKeySenderId[] = "senderId";
const char kKeyNamespace[] = "namespace";
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

bool DeserializeCastMessage(std::string_view buffer,
                            std::string* sender_id,
                            std::string* message_namespace,
                            std::string* message) {
  std::optional<base::Value> converted_value = base::JSONReader::Read(buffer);
  if (!converted_value)
    return false;

  if (!converted_value->is_dict())
    return false;

  const base::Value::Dict& converted_dict = converted_value->GetDict();
  const std::string* sender_id_value = converted_dict.FindString(kKeySenderId);
  if (!sender_id_value)
    return false;
  *sender_id = *sender_id_value;

  const std::string* message_namespace_value =
      converted_dict.FindString(kKeyNamespace);
  if (!message_namespace_value)
    return false;
  *message_namespace = *message_namespace_value;

  const std::string* message_value = converted_dict.FindString(kKeyData);
  if (!message_value)
    return false;
  *message = *message_value;

  return true;
}

std::string SerializeCastMessage(const std::string& sender_id,
                                 const std::string& message_namespace,
                                 const std::string& message) {
  base::Value::Dict value;
  value.Set(kKeyNamespace, message_namespace);
  value.Set(kKeySenderId, sender_id);
  value.Set(kKeyData, message);

  std::string json_message;
  CHECK(base::JSONWriter::Write(value, &json_message));
  return json_message;
}

}  // namespace cast_streaming
