// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_message_port_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/cast_streaming/browser/cast_message_port_converter.h"
#include "components/cast_streaming/common/message_serialization.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace cast_streaming {

namespace {

const char kKeyMediaSessionId[] = "mediaSessionId";
const char kKeyPlaybackRate[] = "playbackRate";
const char kKeyPlayerState[] = "playerState";
const char kKeyCurrentTime[] = "currentTime";
const char kKeySupportedMediaCommands[] = "supportedMediaCommands";
const char kKeyDisableStreamGrouping[] = "disableStreamGrouping";
const char kKeyMedia[] = "media";
const char kKeyContentId[] = "contentId";
const char kKeyStreamType[] = "streamType";
const char kKeyContentType[] = "contentType";
const char kValuePlaying[] = "PLAYING";
const char kValueLive[] = "LIVE";
const char kValueVideoWebm[] = "video/webm";

base::Value::Dict GetMediaCurrentStatusValue() {
  base::Value::Dict media;
  media.Set(kKeyContentId, "");
  media.Set(kKeyStreamType, kValueLive);
  media.Set(kKeyContentType, kValueVideoWebm);

  base::Value::Dict media_current_status;
  media_current_status.Set(kKeyMediaSessionId, 0);
  media_current_status.Set(kKeyPlaybackRate, 1.0);
  media_current_status.Set(kKeyPlayerState, kValuePlaying);
  media_current_status.Set(kKeyCurrentTime, 0);
  media_current_status.Set(kKeySupportedMediaCommands, 0);
  media_current_status.Set(kKeyDisableStreamGrouping, true);
  media_current_status.Set(kKeyMedia, std::move(media));

  return media_current_status;
}

// Implementation of CastMessagePortConverter using CastMessagePortImpl.
class CastMessagePortConverterImpl : public CastMessagePortConverter {
 public:
  CastMessagePortConverterImpl(
      ReceiverSession::MessagePortProvider message_port_provider,
      base::OnceClosure on_close)
      : message_port_(std::move(message_port_provider).Run()),
        on_close_(std::move(on_close)) {}
  ~CastMessagePortConverterImpl() override = default;

  openscreen::cast::MessagePort& GetMessagePort() override {
    if (!openscreen_port_) {
      DCHECK(message_port_);
      openscreen_port_ = std::make_unique<CastMessagePortImpl>(
          std::move(message_port_), std::move(on_close_));
    }
    return *openscreen_port_;
  }

 private:
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  base::OnceClosure on_close_;

  std::unique_ptr<openscreen::cast::MessagePort> openscreen_port_;
};

}  // namespace

std::unique_ptr<CastMessagePortConverter> CastMessagePortConverter::Create(
    ReceiverSession::MessagePortProvider message_port_provider,
    base::OnceClosure on_close) {
  return std::make_unique<CastMessagePortConverterImpl>(
      std::move(message_port_provider), std::move(on_close));
}

CastMessagePortImpl::CastMessagePortImpl(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    base::OnceClosure on_close)
    : message_port_(std::move(message_port)), on_close_(std::move(on_close)) {
  DVLOG(1) << __func__;
  message_port_->SetReceiver(this);

  // Initialize the connection with the Cast Streaming Sender.
  PostMessage(kValueSystemSenderId, kSystemNamespace, kInitialConnectMessage);
}

CastMessagePortImpl::~CastMessagePortImpl() = default;

void CastMessagePortImpl::MaybeClose() {
  if (message_port_) {
    message_port_.reset();
  }
  if (client_) {
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2CastSocketError));
  }
  if (on_close_) {
    // |this| might be deleted as part of |on_close_| being run. Do not add any
    // code after running the closure.
    std::move(on_close_).Run();
  }
}

void CastMessagePortImpl::SetClient(
    openscreen::cast::MessagePort::Client& client) {
  DVLOG(2) << __func__;
  client_ = &client;
}

void CastMessagePortImpl::ResetClient() {
  client_ = nullptr;
  MaybeClose();
}

void CastMessagePortImpl::SendInjectResponse(const std::string& sender_id,
                                             const std::string& message) {
  std::optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": not a json payload:" << message;
    return;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": non-dictionary json payload: " << message;
    return;
  }

  const std::string* type = value->GetDict().FindString(kKeyType);
  if (!type) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no message type: " << message;
    return;
  }
  if (*type != kValueWrapped) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": unknown message type: " << *type;
    return;
  }

  std::optional<int> request_id = value->GetDict().FindInt(kKeyRequestId);
  if (!request_id) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no request id: " << message;
    return;
  }

  // Build the response message.
  base::Value::Dict response_value;
  response_value.Set(kKeyType, kValueError);
  response_value.Set(kKeyRequestId, request_id.value());
  response_value.Set(kKeyData, kValueInjectNotSupportedError);
  response_value.Set(kKeyCode, kValueWrappedError);

  std::string json_message;
  CHECK(base::JSONWriter::Write(response_value, &json_message));
  PostMessage(sender_id, kInjectNamespace, json_message);
}

void CastMessagePortImpl::HandleMediaMessage(const std::string& sender_id,
                                             const std::string& message) {
  std::optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": not a json payload: " << message;
    return;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": non-dictionary json payload: " << message;
    return;
  }

  const std::string* type = value->GetDict().FindString(kKeyType);
  if (!type) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no message type: " << message;
    return;
  }

  if (*type == kValueMediaPlay || *type == kValueMediaPause) {
    // Not supported. Just ignore.
    return;
  }

  if (*type != kValueMediaGetStatus) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": unknown message type: " << *type;
    return;
  }

  std::optional<int> request_id = value->GetDict().FindInt(kKeyRequestId);
  if (!request_id.has_value()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no request id: " << message;
    return;
  }

  base::Value::List message_status_list;
  message_status_list.Append(GetMediaCurrentStatusValue());

  base::Value::Dict response_value;
  response_value.Set(kKeyRequestId, request_id.value());
  response_value.Set(kKeyType, kValueMediaStatus);
  response_value.Set(kKeyStatus, std::move(message_status_list));

  std::string json_message;
  CHECK(base::JSONWriter::Write(response_value, &json_message));
  PostMessage(sender_id, kMediaNamespace, json_message);
}

void CastMessagePortImpl::PostMessage(const std::string& sender_id,
                                      const std::string& message_namespace,
                                      const std::string& message) {
  DVLOG(3) << __func__;
  if (!message_port_)
    return;

  DVLOG(3) << "Received Open Screen message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace << ". Message: " << message;
  message_port_->PostMessage(
      SerializeCastMessage(sender_id, message_namespace, message));
}

bool CastMessagePortImpl::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DVLOG(3) << __func__;

  // If |client_| was cleared, |message_port_| should have been reset.
  DCHECK(client_);

  if (!ports.empty()) {
    // We should never receive any ports for Cast Streaming.
    LOG(ERROR) << "Received ports on Cast Streaming MessagePort.";
    return false;
  }

  std::string sender_id;
  std::string message_namespace;
  std::string str_message;
  if (!DeserializeCastMessage(message, &sender_id, &message_namespace,
                              &str_message)) {
    LOG(ERROR) << "Received bad message.";
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
    return true;
  }
  DVLOG(3) << "Received Cast message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace
           << ". Message: " << str_message;

  // TODO(b/156118960): Have Open Screen handle message namespaces.
  if (message_namespace == kMirroringNamespace ||
      message_namespace == kRemotingNamespace) {
    client_->OnMessage(sender_id, message_namespace, str_message);
  } else if (message_namespace == kInjectNamespace) {
    SendInjectResponse(sender_id, str_message);
  } else if (message_namespace == kMediaNamespace) {
    HandleMediaMessage(sender_id, str_message);
  } else if (message_namespace != kSystemNamespace) {
    // System messages are ignored, log messages from unknown namespaces.
    DVLOG(2) << "Unknown message from " << sender_id
             << ", namespace=" << message_namespace
             << ", message=" << str_message;
  }

  return true;
}

void CastMessagePortImpl::OnPipeError() {
  DVLOG(3) << __func__;
  MaybeClose();
}

}  // namespace cast_streaming
