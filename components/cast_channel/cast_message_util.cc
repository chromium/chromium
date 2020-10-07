// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/cast_channel/cast_auth_util.h"
#include "components/cast_channel/enum_table.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using base::Value;
using cast_util::EnumToString;
using cast_util::StringToEnum;

namespace cast_util {

using ::cast::channel::AuthChallenge;
using ::cast::channel::CastMessage;
using cast_channel::CastMessageType;
using cast_channel::GetAppAvailabilityResult;

template <>
const EnumTable<CastMessageType> EnumTable<CastMessageType>::instance(
    {
        {CastMessageType::kPing, "PING"},
        {CastMessageType::kPong, "PONG"},
        {CastMessageType::kRpc, "RPC"},
        {CastMessageType::kGetAppAvailability, "GET_APP_AVAILABILITY"},
        {CastMessageType::kGetStatus, "GET_STATUS"},
        {CastMessageType::kConnect, "CONNECT"},
        {CastMessageType::kCloseConnection, "CLOSE"},
        {CastMessageType::kBroadcast, "APPLICATION_BROADCAST"},
        {CastMessageType::kLaunch, "LAUNCH"},
        {CastMessageType::kStop, "STOP"},
        {CastMessageType::kReceiverStatus, "RECEIVER_STATUS"},
        {CastMessageType::kMediaStatus, "MEDIA_STATUS"},
        {CastMessageType::kLaunchError, "LAUNCH_ERROR"},
        {CastMessageType::kOffer, "OFFER"},
        {CastMessageType::kAnswer, "ANSWER"},
        {CastMessageType::kCapabilitiesResponse, "CAPABILITIES_RESPONSE"},
        {CastMessageType::kStatusResponse, "STATUS_RESPONSE"},
        {CastMessageType::kMultizoneStatus, "MULTIZONE_STATUS"},
        {CastMessageType::kInvalidPlayerState, "INVALID_PLAYER_STATE"},
        {CastMessageType::kLoadFailed, "LOAD_FAILED"},
        {CastMessageType::kLoadCancelled, "LOAD_CANCELLED"},
        {CastMessageType::kInvalidRequest, "INVALID_REQUEST"},
        {CastMessageType::kPresentation, "PRESENTATION"},
        {CastMessageType::kGetCapabilities, "GET_CAPABILITIES"},
        {CastMessageType::kOther},
    },
    CastMessageType::kMaxValue);

template <>
const EnumTable<cast_channel::V2MessageType>
    EnumTable<cast_channel::V2MessageType>::instance(
        {
            {cast_channel::V2MessageType::kEditTracksInfo, "EDIT_TRACKS_INFO"},
            {cast_channel::V2MessageType::kGetStatus, "GET_STATUS"},
            {cast_channel::V2MessageType::kLoad, "LOAD"},
            {cast_channel::V2MessageType::kMediaGetStatus, "MEDIA_GET_STATUS"},
            {cast_channel::V2MessageType::kMediaSetVolume, "MEDIA_SET_VOLUME"},
            {cast_channel::V2MessageType::kPause, "PAUSE"},
            {cast_channel::V2MessageType::kPlay, "PLAY"},
            {cast_channel::V2MessageType::kPrecache, "PRECACHE"},
            {cast_channel::V2MessageType::kQueueInsert, "QUEUE_INSERT"},
            {cast_channel::V2MessageType::kQueueLoad, "QUEUE_LOAD"},
            {cast_channel::V2MessageType::kQueueRemove, "QUEUE_REMOVE"},
            {cast_channel::V2MessageType::kQueueReorder, "QUEUE_REORDER"},
            {cast_channel::V2MessageType::kQueueUpdate, "QUEUE_UPDATE"},
            {cast_channel::V2MessageType::kQueueNext, "QUEUE_NEXT"},
            {cast_channel::V2MessageType::kQueuePrev, "QUEUE_PREV"},
            {cast_channel::V2MessageType::kSeek, "SEEK"},
            {cast_channel::V2MessageType::kSetVolume, "SET_VOLUME"},
            {cast_channel::V2MessageType::kStop, "STOP"},
            {cast_channel::V2MessageType::kStopMedia, "STOP_MEDIA"},
            {cast_channel::V2MessageType::kOther},
        },
        cast_channel::V2MessageType::kMaxValue);

template <>
const EnumTable<GetAppAvailabilityResult>
    EnumTable<GetAppAvailabilityResult>::instance(
        {
            {GetAppAvailabilityResult::kAvailable, "APP_AVAILABLE"},
            {GetAppAvailabilityResult::kUnavailable, "APP_UNAVAILABLE"},
            {GetAppAvailabilityResult::kUnknown},
        },
        GetAppAvailabilityResult::kMaxValue);

}  // namespace cast_util

namespace cast_channel {

namespace {

constexpr base::StringPiece kCastReservedNamespacePrefix =
    "urn:x-cast:com.google.cast.";

constexpr const char* kReservedNamespaces[] = {
    kAuthNamespace,
    kHeartbeatNamespace,
    kConnectionNamespace,
    kReceiverNamespace,
    kBroadcastNamespace,
    kMediaNamespace,

    // mirroring::mojom::kRemotingNamespace
    "urn:x-cast:com.google.cast.remoting",

    // mirroring::mojom::kWebRtcNamespace
    "urn:x-cast:com.google.cast.webrtc",
};

// The value used for "sdkType" in a virtual connect request. Historically, this
// value is used in the Media Router extension, but here it is reused in Chrome.
constexpr int kVirtualConnectSdkType = 2;

// The value used for "connectionType" in a virtual connect request. This value
// stands for CONNECTION_TYPE_LOCAL, which is the only type used in Chrome.
constexpr int kVirtualConnectTypeLocal = 1;

// The reason code passed to the virtual connection CLOSE message indicating
// that the connection has been gracefully closed by the sender.
constexpr int kVirtualConnectionClosedByPeer = 5;

void FillCommonCastMessageFields(CastMessage* message,
                                 const std::string& source_id,
                                 const std::string& destination_id,
                                 const std::string& message_namespace) {
  message->set_protocol_version(CastMessage::CASTV2_1_0);
  message->set_source_id(source_id);
  message->set_destination_id(destination_id);
  message->set_namespace_(message_namespace);
}

CastMessage CreateKeepAliveMessage(base::StringPiece keep_alive_type) {
  base::Value type_dict(base::Value::Type::DICTIONARY);
  type_dict.SetKey("type", base::Value(keep_alive_type));
  return CreateCastMessage(kHeartbeatNamespace, type_dict, kPlatformSenderId,
                           kPlatformReceiverId);
}

// Returns the value to be set as the "platform" value in a virtual connect
// request. The value is platform-dependent and is taken from the Platform enum
// defined in third_party/metrics_proto/cast_logs.proto.
int GetVirtualConnectPlatformValue() {
#if defined(OS_WIN)
  return 3;
#elif defined(OS_APPLE)
  return 4;
#elif defined(OS_CHROMEOS)
  return 5;
#elif defined(OS_LINUX)
  return 6;
#else
  return 0;
#endif
}

// Maps from from API-internal message types to "real" message types from the
// Cast V2 protocol.  This is necessary because the protocol defines messages
// with the same type in different namespaces, and the namespace is lost when
// messages are passed using a CastInternalMessage object.
base::StringPiece GetRemappedMediaRequestType(
    base::StringPiece v2_message_type) {
  base::Optional<V2MessageType> type =
      StringToEnum<V2MessageType>(v2_message_type);
  DCHECK(type && IsMediaRequestMessageType(*type));
  switch (*type) {
    case V2MessageType::kStopMedia:
      type = V2MessageType::kStop;
      break;
    case V2MessageType::kMediaSetVolume:
      type = V2MessageType::kSetVolume;
      break;
    case V2MessageType::kMediaGetStatus:
      type = V2MessageType::kGetStatus;
      break;
    default:
      return v2_message_type;
  }
  return *EnumToString(*type);
}

}  // namespace

std::ostream& operator<<(std::ostream& lhs, const CastMessage& rhs) {
  lhs << "{";
  if (rhs.has_source_id()) {
    lhs << "source_id: " << rhs.source_id() << ", ";
  }
  if (rhs.has_destination_id()) {
    lhs << "destination_id: " << rhs.destination_id() << ", ";
  }
  if (rhs.has_namespace_()) {
    lhs << "namespace: " << rhs.namespace_() << ", ";
  }
  if (rhs.has_payload_utf8()) {
    lhs << "payload_utf8: " << rhs.payload_utf8();
  }
  if (rhs.has_payload_binary()) {
    lhs << "payload_binary: (" << rhs.payload_binary().size() << " bytes)";
  }
  lhs << "}";
  return lhs;
}

bool IsCastMessageValid(const CastMessage& message_proto) {
  if (!message_proto.IsInitialized())
    return false;

  if (message_proto.namespace_().empty() || message_proto.source_id().empty() ||
      message_proto.destination_id().empty()) {
    return false;
  }
  return (message_proto.payload_type() ==
              cast::channel::CastMessage_PayloadType_STRING &&
          message_proto.has_payload_utf8()) ||
         (message_proto.payload_type() ==
              cast::channel::CastMessage_PayloadType_BINARY &&
          message_proto.has_payload_binary());
}

bool IsCastReservedNamespace(base::StringPiece message_namespace) {
  // Note: Any namespace with the prefix is theoretically reserved for internal
  // messages, but there is at least one namespace in widespread use that uses
  // the "reserved" prefix for app-level messages, so after matching the main
  // prefix, we look for longer prefixes that really need to be reserved.
  if (!base::StartsWith(message_namespace, kCastReservedNamespacePrefix))
    return false;

  const auto prefix_length = kCastReservedNamespacePrefix.length();
  for (base::StringPiece reserved_namespace : kReservedNamespaces) {
    DCHECK(base::StartsWith(reserved_namespace, kCastReservedNamespacePrefix));
    // This comparison skips the first |prefix_length| characters
    // because we already know they match.
    if (base::StartsWith(message_namespace.substr(prefix_length),
                         reserved_namespace.substr(prefix_length)) &&
        // This condition allows |reserved_namespace| to be equal
        // |message_namespace| or be a prefix of it, but if it's a
        // prefix, it must be followed by a dot.  The subscript is
        // never out of bounds because |message_namespace| must be
        // at least as long as |reserved_namespace|.
        (message_namespace.length() == reserved_namespace.length() ||
         message_namespace[reserved_namespace.length()] == '.'))
      return true;
  }
  return false;
}

CastMessageType ParseMessageTypeFromPayload(const base::Value& payload) {
  const Value* type_string = payload.FindKeyOfType("type", Value::Type::STRING);
  return type_string ? CastMessageTypeFromString(type_string->GetString())
                     : CastMessageType::kOther;
}

// TODO(jrw): Eliminate this function.
const char* ToString(CastMessageType message_type) {
  return EnumToString(message_type).value_or("").data();
}

// TODO(jrw): Eliminate this function.
const char* ToString(V2MessageType message_type) {
  return EnumToString(message_type).value_or("").data();
}

// TODO(jrw): Eliminate this function.
CastMessageType CastMessageTypeFromString(const std::string& type) {
  auto result = StringToEnum<CastMessageType>(type);
  DVLOG_IF(1, !result) << "Unknown message type: " << type;
  return result.value_or(CastMessageType::kOther);
}

// TODO(jrw): Eliminate this function.
V2MessageType V2MessageTypeFromString(const std::string& type) {
  return StringToEnum<V2MessageType>(type).value_or(V2MessageType::kOther);
}

// TODO(jrw): Convert to operator<<
std::string AuthMessageToString(const DeviceAuthMessage& message) {
  std::string out("{");
  if (message.has_challenge()) {
    out += "challenge: {}, ";
  }
  if (message.has_response()) {
    out += "response: {signature: (";
    out += base::NumberToString(message.response().signature().length());
    out += " bytes), certificate: (";
    out += base::NumberToString(
        message.response().client_auth_certificate().length());
    out += " bytes)}";
  }
  if (message.has_error()) {
    out += ", error: {";
    out += base::NumberToString(message.error().error_type());
    out += "}";
  }
  out += "}";
  return out;
}

void CreateAuthChallengeMessage(CastMessage* message_proto,
                                const AuthContext& auth_context) {
  CHECK(message_proto);
  DeviceAuthMessage auth_message;

  cast::channel::AuthChallenge* challenge = auth_message.mutable_challenge();
  DCHECK(challenge);
  challenge->set_sender_nonce(auth_context.nonce());
  challenge->set_hash_algorithm(cast::channel::SHA256);

  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  FillCommonCastMessageFields(message_proto, kPlatformSenderId,
                              kPlatformReceiverId, kAuthNamespace);
  message_proto->set_payload_type(
      cast::channel::CastMessage_PayloadType_BINARY);
  message_proto->set_payload_binary(auth_message_string);
}

bool IsAuthMessage(const CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

bool IsReceiverMessage(const CastMessage& message) {
  return message.namespace_() == kReceiverNamespace;
}

bool IsPlatformSenderMessage(const CastMessage& message) {
  return message.destination_id() != cast_channel::kPlatformSenderId;
}

CastMessage CreateKeepAlivePingMessage() {
  return CreateKeepAliveMessage(
      EnumToString<CastMessageType, CastMessageType::kPing>());
}

CastMessage CreateKeepAlivePongMessage() {
  return CreateKeepAliveMessage(
      EnumToString<CastMessageType, CastMessageType::kPong>());
}

CastMessage CreateVirtualConnectionRequest(
    const std::string& source_id,
    const std::string& destination_id,
    VirtualConnectionType connection_type,
    const std::string& user_agent,
    const std::string& browser_version) {
  // Parse system_version from user agent string. It contains platform, OS and
  // CPU info and is contained in the first set of parentheses of the user
  // agent string (e.g., X11; Linux x86_64).
  std::string system_version;
  size_t start_index = user_agent.find('(');
  if (start_index != std::string::npos) {
    size_t end_index = user_agent.find(')', start_index + 1);
    if (end_index != std::string::npos) {
      system_version =
          user_agent.substr(start_index + 1, end_index - start_index - 1);
    }
  }

  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(EnumToString<CastMessageType, CastMessageType::kConnect>()));
  dict.SetKey("userAgent", Value(user_agent));
  dict.SetKey("connType", Value(connection_type));
  dict.SetKey("origin", Value(Value::Type::DICTIONARY));

  Value sender_info(Value::Type::DICTIONARY);
  sender_info.SetKey("sdkType", Value(kVirtualConnectSdkType));
  sender_info.SetKey("version", Value(browser_version));
  sender_info.SetKey("browserVersion", Value(browser_version));
  sender_info.SetKey("platform", Value(GetVirtualConnectPlatformValue()));
  sender_info.SetKey("connectionType", Value(kVirtualConnectTypeLocal));
  if (!system_version.empty())
    sender_info.SetKey("systemVersion", Value(system_version));

  dict.SetKey("senderInfo", std::move(sender_info));

  return CreateCastMessage(kConnectionNamespace, dict, source_id,
                           destination_id);
}

CastMessage CreateVirtualConnectionClose(const std::string& source_id,
                                         const std::string& destination_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(
          EnumToString<CastMessageType, CastMessageType::kCloseConnection>()));
  dict.SetKey("reasonCode", Value(kVirtualConnectionClosedByPeer));
  return CreateCastMessage(kConnectionNamespace, dict, source_id,
                           destination_id);
}

CastMessage CreateGetAppAvailabilityRequest(const std::string& source_id,
                                            int request_id,
                                            const std::string& app_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType,
                                 CastMessageType::kGetAppAvailability>()));
  Value app_id_value(Value::Type::LIST);
  app_id_value.Append(Value(app_id));
  dict.SetKey("appId", std::move(app_id_value));
  dict.SetKey("requestId", Value(request_id));

  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateReceiverStatusRequest(const std::string& source_id,
                                        int request_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(EnumToString<CastMessageType, CastMessageType::kGetStatus>()));
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

BroadcastRequest::BroadcastRequest(const std::string& broadcast_namespace,
                                   const std::string& message)
    : broadcast_namespace(broadcast_namespace), message(message) {}
BroadcastRequest::~BroadcastRequest() = default;

bool BroadcastRequest::operator==(const BroadcastRequest& other) const {
  return broadcast_namespace == other.broadcast_namespace &&
         message == other.message;
}

CastMessage CreateBroadcastRequest(const std::string& source_id,
                                   int request_id,
                                   const std::vector<std::string>& app_ids,
                                   const BroadcastRequest& request) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(EnumToString<CastMessageType, CastMessageType::kBroadcast>()));
  std::vector<Value> app_ids_value;
  for (const std::string& app_id : app_ids)
    app_ids_value.push_back(Value(app_id));

  dict.SetKey("appIds", Value(std::move(app_ids_value)));
  dict.SetKey("namespace", Value(request.broadcast_namespace));
  dict.SetKey("message", Value(request.message));
  return CreateCastMessage(kBroadcastNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateLaunchRequest(
    const std::string& source_id,
    int request_id,
    const std::string& app_id,
    const std::string& locale,
    const std::vector<std::string>& supported_app_types,
    const base::Optional<base::Value>& app_params) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType, CastMessageType::kLaunch>()));
  dict.SetKey("requestId", Value(request_id));
  dict.SetKey("appId", Value(app_id));
  dict.SetKey("language", Value(locale));
  std::vector<Value> supported_app_types_value;
  for (const std::string& type : supported_app_types)
    supported_app_types_value.push_back(Value(type));

  dict.SetKey("supportedAppTypes", Value(supported_app_types_value));
  if (app_params) {
    dict.SetKey("appParams", app_params.value().Clone());
  }
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateStopRequest(const std::string& source_id,
                              int request_id,
                              const std::string& session_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType, CastMessageType::kStop>()));
  dict.SetKey("requestId", Value(request_id));
  dict.SetKey("sessionId", Value(session_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateCastMessage(const std::string& message_namespace,
                              const base::Value& message,
                              const std::string& source_id,
                              const std::string& destination_id) {
  CastMessage output;
  FillCommonCastMessageFields(&output, source_id, destination_id,
                              message_namespace);
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  if (message.is_string()) {
    // NOTE(jrw): This case is needed to fix crbug.com/149843471, which affects
    // the ability to cast the Shaka player.  Without it, the payload of the
    // first message sent with the urn:x-cast:com.google.shaka.v2 namespace is
    // given an extra level of quoting.  It's not clear whether switching on the
    // JSON type of the message is the right thing to do here, or if this case
    // is simply compensating for some other problem that occurs between here
    // and PresentationConnection::send(string, ...), which receives a value
    // with the correct amount of quotation.
    output.set_payload_utf8(message.GetString());
  } else {
    CHECK(base::JSONWriter::Write(message, output.mutable_payload_utf8()));
  }
  return output;
}

CastMessage CreateMediaRequest(const base::Value& body,
                               int request_id,
                               const std::string& source_id,
                               const std::string& destination_id) {
  Value dict = body.Clone();
  Value* type = dict.FindKeyOfType("type", Value::Type::STRING);
  CHECK(type);
  dict.SetKey("type", Value(GetRemappedMediaRequestType(type->GetString())));
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kMediaNamespace, dict, source_id, destination_id);
}

CastMessage CreateSetVolumeRequest(const base::Value& body,
                                   int request_id,
                                   const std::string& source_id) {
  DCHECK(body.FindKeyOfType("type", Value::Type::STRING) &&
         body.FindKeyOfType("type", Value::Type::STRING)->GetString() ==
             (EnumToString<V2MessageType, V2MessageType::kSetVolume>())
                 .as_string());
  Value dict = body.Clone();
  dict.RemoveKey("sessionId");
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

bool IsMediaRequestMessageType(V2MessageType type) {
  switch (type) {
    case V2MessageType::kEditTracksInfo:
    case V2MessageType::kLoad:
    case V2MessageType::kMediaGetStatus:
    case V2MessageType::kMediaSetVolume:
    case V2MessageType::kPause:
    case V2MessageType::kPlay:
    case V2MessageType::kPrecache:
    case V2MessageType::kQueueInsert:
    case V2MessageType::kQueueLoad:
    case V2MessageType::kQueueRemove:
    case V2MessageType::kQueueReorder:
    case V2MessageType::kQueueUpdate:
    case V2MessageType::kQueueNext:
    case V2MessageType::kQueuePrev:
    case V2MessageType::kSeek:
    case V2MessageType::kStopMedia:
      return true;
    default:
      return false;
  }
}

// TODO(jrw): Eliminate this function.
const char* ToString(GetAppAvailabilityResult result) {
  return EnumToString(result).value_or("").data();
}

base::Optional<int> GetRequestIdFromResponse(const Value& payload) {
  DCHECK(payload.is_dict());

  const Value* request_id_value =
      payload.FindKeyOfType("requestId", Value::Type::INTEGER);
  if (!request_id_value)
    return base::nullopt;
  return request_id_value->GetInt();
}

GetAppAvailabilityResult GetAppAvailabilityResultFromResponse(
    const Value& payload,
    const std::string& app_id) {
  DCHECK(payload.is_dict());
  const Value* availability_value =
      payload.FindPathOfType({"availability", app_id}, Value::Type::STRING);
  if (!availability_value)
    return GetAppAvailabilityResult::kUnknown;

  return StringToEnum<GetAppAvailabilityResult>(availability_value->GetString())
      .value_or(GetAppAvailabilityResult::kUnknown);
}

LaunchSessionResponse::LaunchSessionResponse() = default;
LaunchSessionResponse::LaunchSessionResponse(LaunchSessionResponse&& other) =
    default;
LaunchSessionResponse& LaunchSessionResponse::operator=(
    LaunchSessionResponse&& other) = default;
LaunchSessionResponse::~LaunchSessionResponse() = default;

LaunchSessionResponse GetLaunchSessionResponseError(std::string error_msg) {
  LaunchSessionResponse response;
  response.result = LaunchSessionResponse::Result::kError;
  response.error_msg = std::move(error_msg);
  return response;
}

LaunchSessionResponse GetLaunchSessionResponse(const base::Value& payload) {
  const Value* type_value = payload.FindKeyOfType("type", Value::Type::STRING);
  if (!type_value)
    return LaunchSessionResponse();

  const auto type = CastMessageTypeFromString(type_value->GetString());
  if (type != CastMessageType::kReceiverStatus &&
      type != CastMessageType::kLaunchError)
    return LaunchSessionResponse();

  LaunchSessionResponse response;
  if (type == CastMessageType::kLaunchError) {
    response.result = LaunchSessionResponse::Result::kError;
    return response;
  }

  const Value* receiver_status =
      payload.FindKeyOfType("status", Value::Type::DICTIONARY);
  if (!receiver_status)
    return LaunchSessionResponse();

  response.result = LaunchSessionResponse::Result::kOk;
  response.receiver_status = receiver_status->Clone();
  return response;
}

VirtualConnectionType GetConnectionType(const std::string& destination_id) {
  // VCs to recevier-0 are invisible to the receiver application by design.
  // We create a strong connection because some commands (e.g. LAUNCH) are
  // not accepted from invisible connections.
  return destination_id == kPlatformReceiverId
             ? VirtualConnectionType::kStrong
             : VirtualConnectionType::kInvisible;
}

}  // namespace cast_channel
