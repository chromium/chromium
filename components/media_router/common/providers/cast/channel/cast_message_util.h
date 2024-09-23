// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

class AuthContext;
using ::openscreen::cast::proto::CastMessage;
using ::openscreen::cast::proto::DeviceAuthMessage;

// Reserved message namespaces for internal messages.
static constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
static constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
static constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
static constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
static constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

// Sender and receiver IDs to use for platform messages.
static constexpr char kPlatformSenderId[] = "sender-0";
static constexpr char kPlatformReceiverId[] = "receiver-0";

// User prompts messages to approve/disapprove casting
static constexpr char kWaitingUserResponse[] = "USER_PENDING_AUTHORIZATION";
static constexpr char kUserAllowedStatus[] = "USER_ALLOWED";
static constexpr char kUserNotAllowedError[] = "USER_NOT_ALLOWED";
static constexpr char kNotificationDisabledError[] =
    "USER_NOTIFICATIONS_DISABLED";

// Cast application protocol message types.
enum class CastMessageType {
  // Heartbeat messages.
  kPing,
  kPong,

  // RPC control/status messages used by Media Remoting. These occur at high
  // frequency, up to dozens per second at times, and should not be logged.
  kRpc,

  kGetAppAvailability,
  kGetStatus,

  // Virtual connection request
  kConnect,

  // Close virtual connection
  kCloseConnection,

  // Application broadcast/precache. No longer used.
  kBroadcast,

  // Session launch request
  kLaunch,

  // Session stop request
  kStop,

  kReceiverStatus,
  kMediaStatus,

  // Session launch status from receiver
  kLaunchStatus,

  // error from receiver
  kLaunchError,

  kOffer,
  kAnswer,
  kCapabilitiesResponse,
  kStatusResponse,

  // The following values are part of the protocol but are not currently used.
  kMultizoneStatus,
  kInvalidPlayerState,
  kLoadFailed,
  kLoadCancelled,
  kInvalidRequest,
  kPresentation,
  kGetCapabilities,

  kOther,  // Add new types above |kOther|.
  kMaxValue = kOther,
};

enum class V2MessageType {
  // Request to modify the text tracks style or change the tracks status.
  kEditTracksInfo,

  kGetStatus,
  kLoad,
  kMediaGetStatus,
  kMediaSetVolume,
  kPause,
  kPlay,
  kPrecache,

  // Inserts a list of new media items into the queue.
  kQueueInsert,

  // Loads and optionally starts playback of a new queue of media items.
  kQueueLoad,

  // Removes a list of items from the queue. If the remaining queue is empty,
  // the media session will be terminated.
  kQueueRemove,

  // Reorder a list of media items in the queue.
  kQueueReorder,

  // Updates properties of the media queue, e.g. repeat mode, and properties of
  // the existing items in the media queue.
  kQueueUpdate,

  kQueueNext,
  kQueuePrev,
  kSeek,

  // Device set volume is also 'SET_VOLUME'. Thus, give this a different name.
  // The message will be translate before being sent to the receiver.
  kSetVolume,

  kStop,

  // Stop-media type is 'kStop', which collides with stop-session.
  // Thus, give it a different name.  The message will be translate
  // before being sent to the receiver.
  kStopMedia,

  kOther,  // Add new types above |kOther|.
  kMaxValue = kOther,
};

// Receiver App Type determines App types that can be supported by a Cast media
// source. All Cast media sources support the web type.
// Please keep it in sync with the EnumTable in
// components/media_router/common/providers/cast/cast_media_source.cc.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep it in sync with
// MediaRouterResponseReceiverAppType in tools/metrics/histograms/enums.xml.
enum class ReceiverAppType {
  kOther = 0,

  // Web-based Cast receiver apps. This is supported by all Cast media source
  // by default.
  kWeb = 1,

  // A media source may support launching an Android TV app in addition to a
  // Cast web app.
  kAndroidTv = 2,

  // Do not reorder existing entries, and add new types above |kMaxValue|.
  kMaxValue = kAndroidTv,
};

std::ostream& operator<<(std::ostream& lhs, const CastMessage& rhs);

// Checks if the contents of |message_proto| are valid.
bool IsCastMessageValid(const CastMessage& message_proto);

// Returns true if |message_namespace| is a namespace reserved for internal
// messages.
bool IsCastReservedNamespace(std::string_view message_namespace);

// Returns the value in the "type" field or |kOther| if the field is not found.
// The result is only valid if |payload| is a Cast application protocol message.
CastMessageType ParseMessageTypeFromPayload(const base::Value::Dict& payload);

// Returns a human readable string for |message_type|.
const char* ToString(CastMessageType message_type);
const char* ToString(V2MessageType message_type);

// Returns the CastMessageType for |type|, or |kOther| if it does not
// correspond to a known type.
CastMessageType CastMessageTypeFromString(const std::string& type);

// Returns the V2MessageType for |type|, or |kOther| if it does not
// correspond to a known type.
V2MessageType V2MessageTypeFromString(const std::string& type);

// Returns a human readable string for |message|.  Should probably be converted
// to operator<<.
std::string AuthMessageToString(const DeviceAuthMessage& message);

// Fills |message_proto| appropriately for an auth challenge request message.
// Uses the nonce challenge in |auth_context|.
void CreateAuthChallengeMessage(CastMessage* message_proto,
                                const AuthContext& auth_context);

// Returns whether the given message is an auth handshake message.
bool IsAuthMessage(const CastMessage& message);

// Returns whether |message| is a Cast receiver message.
bool IsReceiverMessage(const CastMessage& message);

// Returns whether |message| is destined for the platform sender.
bool IsPlatformSenderMessage(const CastMessage& message);

// Creates a keep-alive message of either type PING or PONG.
CastMessage CreateKeepAlivePingMessage();
CastMessage CreateKeepAlivePongMessage();

enum VirtualConnectionType {
  kStrong = 0,
  // kWeak = 1 not used.
  kInvisible = 2
};

// Creates a virtual connection request message for |source_id| and
// |destination_id|. |user_agent| and |browser_version| will be included with
// the request.
// If |destination_id| is kPlatformReceiverId, then |connection_type| must be
// kStrong. Otherwise |connection_type| can be either kStrong or kInvisible.
CastMessage CreateVirtualConnectionRequest(
    const std::string& source_id,
    const std::string& destination_id,
    VirtualConnectionType connection_type,
    const std::string& user_agent,
    const std::string& browser_version);

CastMessage CreateVirtualConnectionClose(const std::string& source_id,
                                         const std::string& destination_id);

// Creates an app availability request for |app_id| from |source_id| with
// ID |request_id|.
// TODO(imcheng): May not need |source_id|, just use sender-0?
CastMessage CreateGetAppAvailabilityRequest(const std::string& source_id,
                                            int request_id,
                                            const std::string& app_id);

CastMessage CreateReceiverStatusRequest(const std::string& source_id,
                                        int request_id);

// Creates a session launch request with the given parameters.
CastMessage CreateLaunchRequest(
    const std::string& source_id,
    int request_id,
    const std::string& app_id,
    const std::string& locale,
    const std::vector<std::string>& supported_app_types,
    const std::optional<base::Value>& app_params);

CastMessage CreateStopRequest(const std::string& source_id,
                              int request_id,
                              const std::string& session_id);

// Creates a generic CastMessage with |message| as the string payload. Used for
// app messages.
CastMessage CreateCastMessage(const std::string& message_namespace,
                              const base::Value& message,
                              const std::string& source_id,
                              const std::string& destination_id);

CastMessage CreateMediaRequest(const base::Value::Dict& body,
                               int request_id,
                               const std::string& source_id,
                               const std::string& destination_id);

CastMessage CreateSetVolumeRequest(const base::Value::Dict& body,
                                   int request_id,
                                   const std::string& source_id);

bool IsMediaRequestMessageType(V2MessageType v2_message_type);

// Possible results of a GET_APP_AVAILABILITY request.
enum class GetAppAvailabilityResult {
  kAvailable,
  kUnavailable,
  kUnknown,
  kMaxValue = kUnknown,
};

const char* ToString(GetAppAvailabilityResult result);

// Extracts request ID from |payload| corresponding to a Cast message response.
std::optional<int> GetRequestIdFromResponse(const base::Value::Dict& payload);

// Returns the GetAppAvailabilityResult corresponding to |app_id| in |payload|.
// Returns kUnknown if result is not found.
GetAppAvailabilityResult GetAppAvailabilityResultFromResponse(
    const base::Value::Dict& payload,
    const std::string& app_id);

// Result of a session launch.
struct LaunchSessionResponse {
  enum Result {
    kOk,
    kError,
    kTimedOut,
    kPendingUserAuth,  // Indicates waiting for the user response to allow or
                       // reject a cast request.
    kUserAllowed,  // Indicates that casting will start as the user allowed the
                   // cast request.
    kUserNotAllowed,  // Indicates that casting didn't start because the user
                      // rejected the cast request.
    kNotificationDisabled,  // Indicates that casting didn't start because
                            // notifications are disabled on the receiver
                            // device.
    kUnknown,
    kMaxValue = kUnknown
  };

  LaunchSessionResponse();
  LaunchSessionResponse(const LaunchSessionResponse& other) = delete;
  LaunchSessionResponse(LaunchSessionResponse&& other);
  LaunchSessionResponse& operator=(const LaunchSessionResponse& other) = delete;
  LaunchSessionResponse& operator=(LaunchSessionResponse&& other);
  ~LaunchSessionResponse();

  Result result = Result::kUnknown;
  // Populated if |result| is |kOk|.
  std::optional<base::Value::Dict> receiver_status;
  // Populated if |result| is |kError|.
  std::string error_msg;
};

// Parses |payload| into a LaunchSessionResponse. Returns an empty
// LaunchSessionResponse if |payload| is not a properly formatted launch
// response. |payload| must be a dictionary from the string payload of a
// CastMessage.
LaunchSessionResponse GetLaunchSessionResponse(
    const base::Value::Dict& payload);

LaunchSessionResponse GetLaunchSessionResponseError(std::string error_msg);

// Returns what connection type should be used based on the destination ID.
VirtualConnectionType GetConnectionType(const std::string& destination_id);

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
