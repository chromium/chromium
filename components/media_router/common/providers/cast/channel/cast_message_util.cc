// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_message_util.h"

#include <memory>
#include <string_view>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using base::Value;
using cast_util::EnumToString;
using cast_util::StringToEnum;

namespace cast_util {

using cast_channel::CastMessageType;
using cast_channel::GetAppAvailabilityResult;
using ::openscreen::cast::proto::AuthChallenge;
using ::openscreen::cast::proto::CastMessage;

template <>
const EnumTable<CastMessageType>& EnumTable<CastMessageType>::GetInstance() {
  static const EnumTable<CastMessageType> kInstance(
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
          {CastMessageType::kLaunchStatus, "LAUNCH_STATUS"},
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
  return kInstance;
}

template <>
const EnumTable<cast_channel::V2MessageType>&
EnumTable<cast_channel::V2MessageType>::GetInstance() {
  static const EnumTable<cast_channel::V2MessageType> kInstance(
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
  return kInstance;
}

template <>
const EnumTable<GetAppAvailabilityResult>&
EnumTable<GetAppAvailabilityResult>::GetInstance() {
  static const EnumTable<GetAppAvailabilityResult> kInstance(
      {
          {GetAppAvailabilityResult::kAvailable, "APP_AVAILABLE"},
          {GetAppAvailabilityResult::kUnavailable, "APP_UNAVAILABLE"},
          {GetAppAvailabilityResult::kUnknown},
      },
      GetAppAvailabilityResult::kMaxValue);
  return kInstance;
}

}  // namespace cast_util

namespace cast_channel {

namespace {

constexpr std::string_view kCastReservedNamespacePrefix =
    "urn:x-cast:com.google.cast.";

constexpr const char* kReservedNamespaces[] = {
    kAuthNamespace,
    kHeartbeatNamespace,
    kConnectionNamespace,
    kReceiverNamespace,
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

CastMessage CreateKeepAliveMessage(std::string_view keep_alive_type) {
  base::Value::Dict type_dict;
  type_dict.Set("type", keep_alive_type);
  return CreateCastMessage(kHeartbeatNamespace,
                           base::Value(std::move(type_dict)), kPlatformSenderId,
                           kPlatformReceiverId);
}

// Returns the value to be set as the "platform" value in a virtual connect
// request. The value is platform-dependent and is taken from the Platform enum
// defined in third_party/metrics_proto/cast_logs.proto.
int GetVirtualConnectPlatformValue() {
#if BUILDFLAG(IS_WIN)
  return 3;
#elif BUILDFLAG(IS_APPLE)
  return 4;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return 5;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return 6;
#else
  return 0;
#endif
}

// Maps from from API-internal message types to "real" message types from the
// Cast V2 protocol.  This is necessary because the protocol defines messages
// with the same type in different namespaces, and the namespace is lost when
// messages are passed using a CastInternalMessage object.
std::string_view GetRemappedMediaRequestType(std::string_view v2_message_type) {
  std::optional<V2MessageType> type =
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
              openscreen::cast::proto::CastMessage_PayloadType_STRING &&
          message_proto.has_payload_utf8()) ||
         (message_proto.payload_type() ==
              openscreen::cast::proto::CastMessage_PayloadType_BINARY &&
          message_proto.has_payload_binary());
}

bool IsCastReservedNamespace(std::string_view message_namespace) {
  // Note: Any namespace with the prefix is theoretically reserved for internal
  // messages, but there is at least one namespace in widespread use that uses
  // the "reserved" prefix for app-level messages, so after matching the main
  // prefix, we look for longer prefixes that really need to be reserved.
  if (!base::StartsWith(message_namespace, kCastReservedNamespacePrefix))
    return false;

  const auto prefix_length = kCastReservedNamespacePrefix.length();
  for (std::string_view reserved_namespace : kReservedNamespaces) {
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

CastMessageType ParseMessageTypeFromPayload(const base::Value::Dict& payload) {
  const std::string* type_string = payload.FindString("type");
  return type_string ? CastMessageTypeFromString(*type_string)
                     : CastMessageType::kOther;
}

// TODO(crbug.com/1291730): Eliminate this function.
const char* ToString(CastMessageType message_type) {
  return EnumToString(message_type).value_or("").data();
}

// TODO(crbug.com/1291730): Eliminate this function.
const char* ToString(V2MessageType message_type) {
  return EnumToString(message_type).value_or("").data();
}

// TODO(crbug.com/1291730): Eliminate this function.
CastMessageType CastMessageTypeFromString(const std::string& type) {
  auto result = StringToEnum<CastMessageType>(type);
  DVLOG_IF(1, !result) << "Unknown message type: " << type;
  return result.value_or(CastMessageType::kOther);
}

// TODO(crbug.com/1291730): Eliminate this function.
V2MessageType V2MessageTypeFromString(const std::string& type) {
  return StringToEnum<V2MessageType>(type).value_or(V2MessageType::kOther);
}

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

  openscreen::cast::proto::AuthChallenge* challenge =
      auth_message.mutable_challenge();
  DCHECK(challenge);
  challenge->set_sender_nonce(auth_context.nonce());
  challenge->set_hash_algorithm(openscreen::cast::proto::SHA256);

  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  FillCommonCastMessageFields(message_proto, kPlatformSenderId,
                              kPlatformReceiverId, kAuthNamespace);
  message_proto->set_payload_type(
      openscreen::cast::proto::CastMessage_PayloadType_BINARY);
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

  Value::Dict dict;
  dict.Set("type", EnumToString<CastMessageType, CastMessageType::kConnect>());
  dict.Set("userAgent", user_agent);
  dict.Set("connType", connection_type);
  dict.Set("origin", base::Value::Dict());

  Value::Dict sender_info;
  sender_info.Set("sdkType", kVirtualConnectSdkType);
  sender_info.Set("version", browser_version);
  sender_info.Set("browserVersion", browser_version);
  sender_info.Set("platform", GetVirtualConnectPlatformValue());
  sender_info.Set("connectionType", kVirtualConnectTypeLocal);
  if (!system_version.empty())
    sender_info.Set("systemVersion", system_version);

  dict.Set("senderInfo", std::move(sender_info));

  return CreateCastMessage(kConnectionNamespace, base::Value(std::move(dict)),
                           source_id, destination_id);
}

CastMessage CreateVirtualConnectionClose(const std::string& source_id,
                                         const std::string& destination_id) {
  Value::Dict dict;
  dict.Set("type",
           EnumToString<CastMessageType, CastMessageType::kCloseConnection>());
  dict.Set("reasonCode", kVirtualConnectionClosedByPeer);
  return CreateCastMessage(kConnectionNamespace, base::Value(std::move(dict)),
                           source_id, destination_id);
}

CastMessage CreateGetAppAvailabilityRequest(const std::string& source_id,
                                            int request_id,
                                            const std::string& app_id) {
  Value::Dict dict;
  dict.Set(
      "type",
      EnumToString<CastMessageType, CastMessageType::kGetAppAvailability>());
  Value::List app_id_value;
  app_id_value.Append(app_id);
  dict.Set("appId", std::move(app_id_value));
  dict.Set("requestId", request_id);

  return CreateCastMessage(kReceiverNamespace, base::Value(std::move(dict)),
                           source_id, kPlatformReceiverId);
}

CastMessage CreateReceiverStatusRequest(const std::string& source_id,
                                        int request_id) {
  Value::Dict dict;
  dict.Set("type",
           EnumToString<CastMessageType, CastMessageType::kGetStatus>());
  dict.Set("requestId", request_id);
  return CreateCastMessage(kReceiverNamespace, base::Value(std::move(dict)),
                           source_id, kPlatformReceiverId);
}

CastMessage CreateLaunchRequest(
    const std::string& source_id,
    int request_id,
    const std::string& app_id,
    const std::string& locale,
    const std::vector<std::string>& supported_app_types,
    const std::optional<base::Value>& app_params) {
  Value::Dict dict;
  dict.Set("type", EnumToString<CastMessageType, CastMessageType::kLaunch>());
  dict.Set("requestId", request_id);
  dict.Set("appId", app_id);
  dict.Set("language", locale);
  base::Value::List supported_app_types_value;
  for (const std::string& type : supported_app_types)
    supported_app_types_value.Append(type);

  dict.Set("supportedAppTypes", std::move(supported_app_types_value));
  if (app_params)
    dict.Set("appParams", app_params.value().Clone());
  return CreateCastMessage(kReceiverNamespace, base::Value(std::move(dict)),
                           source_id, kPlatformReceiverId);
}

CastMessage CreateStopRequest(const std::string& source_id,
                              int request_id,
                              const std::string& session_id) {
  Value::Dict dict;
  dict.Set("type", EnumToString<CastMessageType, CastMessageType::kStop>());
  dict.Set("requestId", request_id);
  dict.Set("sessionId", session_id);
  return CreateCastMessage(kReceiverNamespace, base::Value(std::move(dict)),
                           source_id, kPlatformReceiverId);
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

CastMessage CreateMediaRequest(const base::Value::Dict& body,
                               int request_id,
                               const std::string& source_id,
                               const std::string& destination_id) {
  Value::Dict dict = body.Clone();
  std::string* type = dict.FindString("type");
  CHECK(type);
  dict.Set("type", GetRemappedMediaRequestType(*type));
  dict.Set("requestId", request_id);
  return CreateCastMessage(kMediaNamespace, base::Value(std::move(dict)),
                           source_id, destination_id);
}

CastMessage CreateSetVolumeRequest(const base::Value::Dict& body,
                                   int request_id,
                                   const std::string& source_id) {
  DCHECK(body.FindString("type") &&
         *body.FindString("type") ==
             (EnumToString<V2MessageType, V2MessageType::kSetVolume>()));
  Value::Dict dict = body.Clone();
  dict.Remove("sessionId");
  dict.Set("requestId", request_id);
  return CreateCastMessage(kReceiverNamespace, base::Value(std::move(dict)),
                           source_id, kPlatformReceiverId);
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

// TODO(crbug.com/1291730): Eliminate this function.
const char* ToString(GetAppAvailabilityResult result) {
  return EnumToString(result).value_or("").data();
}

std::optional<int> GetRequestIdFromResponse(const Value::Dict& payload) {
  std::optional<int> request_id = payload.FindInt("requestId");
  return request_id ? request_id : payload.FindInt("launchRequestId");
}

GetAppAvailabilityResult GetAppAvailabilityResultFromResponse(
    const Value::Dict& payload,
    const std::string& app_id) {
  const Value::Dict* availability_dict = payload.FindDict("availability");
  if (!availability_dict)
    return GetAppAvailabilityResult::kUnknown;
  const std::string* availability = availability_dict->FindString(app_id);
  if (!availability)
    return GetAppAvailabilityResult::kUnknown;

  return StringToEnum<GetAppAvailabilityResult>(*availability)
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

LaunchSessionResponse GetLaunchSessionResponse(
    const base::Value::Dict& payload) {
  const std::string* type_string = payload.FindString("type");
  if (!type_string)
    return LaunchSessionResponse();

  const auto type = CastMessageTypeFromString(*type_string);
  if (type != CastMessageType::kReceiverStatus &&
      type != CastMessageType::kLaunchError &&
      type != CastMessageType::kLaunchStatus) {
    return LaunchSessionResponse();
  }

  LaunchSessionResponse response;

  if (type == CastMessageType::kLaunchError) {
    const std::string* extended_error = payload.FindString("extendedError");

    if (extended_error && *extended_error == kUserNotAllowedError) {
      response.result = LaunchSessionResponse::Result::kUserNotAllowed;
    } else if (extended_error &&
               *extended_error == kNotificationDisabledError) {
      response.result = LaunchSessionResponse::Result::kNotificationDisabled;
    } else {
      response.result = LaunchSessionResponse::Result::kError;
      const std::string* error_msg = payload.FindString("error");
      response.error_msg = (error_msg ? *error_msg : "");
    }
    return response;
  }

  if (type == CastMessageType::kLaunchStatus) {
    const std::string* launch_status = payload.FindString("status");

    if (launch_status && *launch_status == kUserAllowedStatus) {
      response.result = LaunchSessionResponse::Result::kUserAllowed;
    } else if (launch_status && *launch_status == kWaitingUserResponse) {
      response.result = LaunchSessionResponse::Result::kPendingUserAuth;
    }
    return response;
  }

  const Value::Dict* receiver_status = payload.FindDict("status");
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
