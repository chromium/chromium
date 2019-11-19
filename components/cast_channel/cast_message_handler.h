// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_
#define COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "base/token.h"
#include "base/values.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace cast_channel {

class CastSocketService;

template <typename CallbackType>
struct PendingRequest {
 public:
  PendingRequest(int request_id,
                 CallbackType callback,
                 const base::TickClock* clock)
      : request_id(request_id),
        callback(std::move(callback)),
        timeout_timer(clock) {}

  virtual ~PendingRequest() = default;

  int request_id;
  CallbackType callback;
  base::OneShotTimer timeout_timer;
};

// |app_id|: ID of app the result is for.
// |result|: Availability result from the receiver.
using GetAppAvailabilityCallback =
    base::OnceCallback<void(const std::string& app_id,
                            GetAppAvailabilityResult result)>;

// Represents an app availability request to a Cast sink.
struct GetAppAvailabilityRequest
    : public PendingRequest<GetAppAvailabilityCallback> {
 public:
  GetAppAvailabilityRequest(int request_id,
                            GetAppAvailabilityCallback callback,
                            const base::TickClock* clock,
                            const std::string& app_id);
  ~GetAppAvailabilityRequest() override;

  // App ID of the request.
  std::string app_id;
};

// Represents an app launch request to a Cast sink.
using LaunchSessionCallback =
    base::OnceCallback<void(LaunchSessionResponse response)>;
using LaunchSessionRequest = PendingRequest<LaunchSessionCallback>;

enum class Result { kOk, kFailed };
using ResultCallback = base::OnceCallback<void(Result result)>;

// Represents an app stop request to a Cast sink.
using StopSessionRequest = PendingRequest<ResultCallback>;

// Reresents request for a sink to set its volume level.
using SetVolumeRequest = PendingRequest<ResultCallback>;

// Represents a virtual connection on a cast channel. A virtual connection is
// given by a source and destination ID pair, and must be created before
// messages can be sent. Virtual connections are managed by CastMessageHandler.
struct VirtualConnection {
  VirtualConnection(int channel_id,
                    const std::string& source_id,
                    const std::string& destination_id);
  ~VirtualConnection();

  bool operator<(const VirtualConnection& other) const;

  // ID of cast channel.
  int channel_id;

  // Source ID (e.g. sender-0).
  std::string source_id;

  // Destination ID (e.g. receiver-0).
  std::string destination_id;
};

struct InternalMessage {
  InternalMessage(CastMessageType type,
                  const std::string& message_namespace,
                  base::Value message);
  ~InternalMessage();

  CastMessageType type;
  // TODO(jrw): This field is only needed to communicate the namespace
  // information from CastMessageHandler::OnMessage to
  // MirroringActivityRecord::OnInternalMessage.  Maybe there's a better way?
  // One possibility is to derive namespace when it's needed based on the
  // context and/or message type.
  std::string message_namespace;
  base::Value message;
};

// Default timeout amount for requests waiting for a response.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(5);

// Handles messages that are sent between this browser instance and the Cast
// devices connected to it. This class also manages virtual connections (VCs)
// with each connected Cast device and ensures a proper VC exists before the
// message is sent. This makes the concept of VC transparent to the client.
// This class may be created on any sequence, but other methods (including
// destructor) must be run on the same sequence that CastSocketService runs on.
class CastMessageHandler : public CastSocket::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnAppMessage(int channel_id, const CastMessage& message) {}
    virtual void OnInternalMessage(int channel_id,
                                   const InternalMessage& message) {}
  };

  // |parse_json|: A callback which can be used to parse a string of potentially
  // unsafe JSON data.
  using ParseJsonCallback = base::RepeatingCallback<void(
      const std::string& string,
      data_decoder::DataDecoder::ValueParseCallback callback)>;
  CastMessageHandler(CastSocketService* socket_service,
                     ParseJsonCallback parse_json,
                     const std::string& user_agent,
                     const std::string& browser_version,
                     const std::string& locale);
  ~CastMessageHandler() override;

  // Ensures a virtual connection exists for (|source_id|, |destination_id|) on
  // the device given by |channel_id|, sending a virtual connection request to
  // the device if necessary. Although a virtual connection is automatically
  // created when sending a message, a caller may decide to create it beforehand
  // in order to receive messages sooner.
  virtual void EnsureConnection(int channel_id,
                                const std::string& source_id,
                                const std::string& destination_id);

  // Sends an app availability for |app_id| to the device given by |socket|.
  // |callback| is always invoked asynchronously, and will be invoked when a
  // response is received, or if the request timed out. No-ops if there is
  // already a pending request with the same socket and app ID.
  virtual void RequestAppAvailability(CastSocket* socket,
                                      const std::string& app_id,
                                      GetAppAvailabilityCallback callback);

  // Sends a receiver status request to the socket given by |channel_id|.
  virtual void RequestReceiverStatus(int channel_id);

  // Sends a broadcast message containing |app_ids| and |request| to the socket
  // given by |channel_id|.
  virtual void SendBroadcastMessage(int channel_id,
                                    const std::vector<std::string>& app_ids,
                                    const BroadcastRequest& request);

  // Requests a session launch for |app_id| on the device given by |channel_id|.
  // |callback| will be invoked with the response or with a timed out result if
  // no response comes back before |launch_timeout|.
  virtual void LaunchSession(int channel_id,
                             const std::string& app_id,
                             base::TimeDelta launch_timeout,
                             LaunchSessionCallback callback);

  // Stops the session given by |session_id| on the device given by
  // |channel_id|. |callback| will be invoked with the result of the stop
  // request.
  virtual void StopSession(int channel_id,
                           const std::string& session_id,
                           const base::Optional<std::string>& client_id,
                           ResultCallback callback);

  // Sends |message| to the device given by |channel_id|. The caller may use
  // this method to forward app messages from the SDK client to the device.
  //
  // TODO(jrw): Could this be merged with SendAppMessage()?  Note from mfoltz:
  //
  // The two differences between an app message and a protocol message:
  // - app message has a sender ID that comes from the clientId of the SDK
  // - app message has a custom (non-Cast) namespace
  //
  // So if you added senderId to CastMessage, it seems like you could have one
  // method for both.
  virtual Result SendCastMessage(int channel_id, const CastMessage& message);

  // Sends |message| to the device given by |channel_id|. The caller may use
  // this method to forward app messages from the SDK client to the device. It
  // is invalid to call this method with a message in one of the Cast internal
  // message namespaces.
  virtual Result SendAppMessage(int channel_id, const CastMessage& message);

  // Sends a media command |body|. Returns the ID of the request that is sent to
  // the receiver. It is invalid to call this with a message body that is not a
  // media command.  Returns |base::nullopt| if |channel_id| is invalid.
  //
  // Note: This API is designed to return a request ID instead of taking a
  // callback. This is because a MEDIA_STATUS message from the receiver can be
  // the response to a media command from a client. Thus when we get a
  // MEDIA_STATUS message, we need to be able to (1) broadcast the message to
  // all clients and (2) make sure the client that sent the media command
  // receives the message only once *and* in the form of a response (by setting
  // the sequenceNumber on the message).
  virtual base::Optional<int> SendMediaRequest(
      int channel_id,
      const base::Value& body,
      const std::string& source_id,
      const std::string& destination_id);

  // Sends a set system volume command |body|. |callback| will be invoked
  // with the result of the operation. It is invalid to call this with
  // a message body that is not a volume request.
  virtual void SendSetVolumeRequest(int channel_id,
                                    const base::Value& body,
                                    const std::string& source_id,
                                    ResultCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // CastSocket::Observer implementation.
  void OnError(const CastSocket& socket, ChannelError error_state) override;
  void OnMessage(const CastSocket& socket, const CastMessage& message) override;
  void OnReadyStateChanged(const CastSocket& socket) override;

  const std::string& sender_id() const { return sender_id_; }

 private:
  friend class CastMessageHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CastMessageHandlerTest, HandlePendingRequest);

  // Set of PendingRequests for a CastSocket.
  class PendingRequests {
   public:
    PendingRequests();
    ~PendingRequests();

    // Returns true if this is the first request for the given app ID.
    bool AddAppAvailabilityRequest(
        std::unique_ptr<GetAppAvailabilityRequest> request);

    bool AddLaunchRequest(std::unique_ptr<LaunchSessionRequest> request,
                          base::TimeDelta timeout);
    bool AddStopRequest(std::unique_ptr<StopSessionRequest> request);
    void AddVolumeRequest(std::unique_ptr<SetVolumeRequest> request);
    void HandlePendingRequest(int request_id, const base::Value& response);

   private:
    // Invokes the pending callback associated with |request_id| with a timed
    // out result.
    void AppAvailabilityTimedOut(int request_id);
    void LaunchSessionTimedOut(int request_id);
    void StopSessionTimedOut(int request_id);
    void SetVolumeTimedOut(int request_id);

    // Requests are kept in the order in which they were created.
    std::vector<std::unique_ptr<GetAppAvailabilityRequest>>
        pending_app_availability_requests_;
    std::unique_ptr<LaunchSessionRequest> pending_launch_session_request_;
    std::unique_ptr<StopSessionRequest> pending_stop_session_request_;
    base::flat_map<int, std::unique_ptr<SetVolumeRequest>>
        pending_volume_requests_by_id_;
  };

  // Used internally to generate the next ID to use in a request type message.
  // Returns a positive integer (unless the counter overflows).
  int NextRequestId() { return ++next_request_id_; }

  PendingRequests* GetOrCreatePendingRequests(int channel_id);

  // Sends |message| over |socket|. This also ensures the necessary virtual
  // connection exists before sending the message.
  void SendCastMessageToSocket(CastSocket* socket, const CastMessage& message);

  // Sends a virtual connection request to |socket| if the virtual connection
  // for (|source_id|, |destination_id|) does not yet exist.
  void DoEnsureConnection(CastSocket* socket,
                          const std::string& source_id,
                          const std::string& destination_id);

  // Callback for CastTransport::SendMessage.
  void OnMessageSent(int result);

  void HandleCastInternalMessage(
      int channel_id,
      const std::string& source_id,
      const std::string& destination_id,
      const std::string& namespace_,
      data_decoder::DataDecoder::ValueOrError parse_result);

  // Set of pending requests keyed by socket ID.
  base::flat_map<int, std::unique_ptr<PendingRequests>> pending_requests_;

  // Source ID used for platform messages. The suffix is randomized to
  // distinguish it from other Cast senders on the same network.
  const std::string sender_id_;

  // Used for parsing JSON payload from receivers.
  ParseJsonCallback parse_json_;

  // User agent and browser version strings included in virtual connection
  // messages.
  const std::string user_agent_;
  const std::string browser_version_;

  // Locale string used for session launch requests.
  const std::string locale_;

  int next_request_id_ = 0;

  base::ObserverList<Observer>::Unchecked observers_;

  // Set of virtual connections opened to receivers.
  base::flat_set<VirtualConnection> virtual_connections_;

  CastSocketService* const socket_service_;

  // Non-owned pointer to TickClock used for request timeouts.
  const base::TickClock* const clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastMessageHandler);
};

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_
