// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/types/expected_macros.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_metrics.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"

namespace cast_channel {

namespace {

// The max launch timeout amount for session launch requests.
constexpr base::TimeDelta kLaunchMaxTimeout = base::Minutes(2);

// The max size of Cast Message is 64KB.
constexpr int kMaxCastMessagePayload = 64 * 1024;

void ReportParseError(const std::string& error) {
  DVLOG(1) << "Error parsing JSON message: " << error;
}

}  // namespace

GetAppAvailabilityRequest::GetAppAvailabilityRequest(
    int request_id,
    GetAppAvailabilityCallback callback,
    const base::TickClock* clock,
    const std::string& app_id)
    : PendingRequest(request_id, std::move(callback), clock), app_id(app_id) {}

GetAppAvailabilityRequest::~GetAppAvailabilityRequest() = default;

LaunchSessionCallbackWrapper::LaunchSessionCallbackWrapper() = default;
LaunchSessionCallbackWrapper::~LaunchSessionCallbackWrapper() = default;

VirtualConnection::VirtualConnection(int channel_id,
                                     std::string_view source_id,
                                     std::string_view destination_id)
    : channel_id(channel_id),
      source_id(source_id),
      destination_id(destination_id) {}
VirtualConnection::~VirtualConnection() = default;

bool VirtualConnection::operator<(const VirtualConnection& other) const {
  return std::tie(channel_id, source_id, destination_id) <
         std::tie(other.channel_id, other.source_id, other.destination_id);
}

InternalMessage::InternalMessage(CastMessageType type,
                                 std::string_view source_id,
                                 std::string_view destination_id,
                                 std::string_view message_namespace,
                                 base::Value::Dict message)
    : type(type),
      source_id(source_id),
      destination_id(destination_id),
      message_namespace(message_namespace),
      message(std::move(message)) {}
InternalMessage::~InternalMessage() = default;

CastMessageHandler::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

CastMessageHandler::CastMessageHandler(CastSocketService* socket_service,
                                       ParseJsonCallback parse_json,
                                       std::string_view user_agent,
                                       std::string_view browser_version,
                                       std::string_view locale)
    : source_id_(base::StringPrintf("sender-%d", base::RandInt(0, 1000000))),
      parse_json_(std::move(parse_json)),
      user_agent_(user_agent),
      browser_version_(browser_version),
      locale_(locale),
      socket_service_(socket_service),
      clock_(base::DefaultTickClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  socket_service_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastSocketService::AddObserver,
                                base::Unretained(socket_service_),
                                base::Unretained(this)));
}

CastMessageHandler::~CastMessageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  socket_service_->RemoveObserver(this);
}

void CastMessageHandler::EnsureConnection(
    int channel_id,
    const std::string& source_id,
    const std::string& destination_id,
    VirtualConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << "Socket not found: " << channel_id;
    return;
  }

  DoEnsureConnection(socket, source_id, destination_id, connection_type);
}

void CastMessageHandler::CloseConnection(int channel_id,
                                         const std::string& source_id,
                                         const std::string& destination_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    return;
  }

  VirtualConnection connection(socket->id(), source_id, destination_id);
  if (virtual_connections_.find(connection) == virtual_connections_.end()) {
    return;
  }
  VLOG(1) << "Closing VC for channel: " << connection.channel_id
          << ", source: " << connection.source_id
          << ", dest: " << connection.destination_id;
  // Assume the virtual connection close will succeed.  Eventually the receiver
  // will remove the connection even if it doesn't succeed.
  socket->transport()->SendMessage(
      CreateVirtualConnectionClose(connection.source_id,
                                   connection.destination_id),
      base::BindOnce(&CastMessageHandler::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr()));

  RemoveConnection(channel_id, source_id, destination_id);
}

void CastMessageHandler::RemoveConnection(int channel_id,
                                          const std::string& source_id,
                                          const std::string& destination_id) {
  virtual_connections_.erase(
      VirtualConnection(channel_id, source_id, destination_id));
}

CastMessageHandler::PendingRequests*
CastMessageHandler::GetOrCreatePendingRequests(int channel_id) {
  CastMessageHandler::PendingRequests* requests = nullptr;
  auto pending_it = pending_requests_.find(channel_id);
  if (pending_it != pending_requests_.end()) {
    return pending_it->second.get();
  }

  auto new_requests = std::make_unique<CastMessageHandler::PendingRequests>();
  requests = new_requests.get();
  pending_requests_.emplace(channel_id, std::move(new_requests));
  return requests;
}

void CastMessageHandler::RequestAppAvailability(
    CastSocket* socket,
    const std::string& app_id,
    GetAppAvailabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int channel_id = socket->id();
  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();

  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", app_id: " << app_id << ", request_id: " << request_id;
  if (requests->AddAppAvailabilityRequest(
          std::make_unique<GetAppAvailabilityRequest>(
              request_id, std::move(callback), clock_, app_id))) {
    SendCastMessageToSocket(socket, CreateGetAppAvailabilityRequest(
                                        source_id_, request_id, app_id));
  }
}

void CastMessageHandler::RequestReceiverStatus(int channel_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  int request_id = NextRequestId();
  SendCastMessageToSocket(socket,
                          CreateReceiverStatusRequest(source_id_, request_id));
}

void CastMessageHandler::LaunchSession(
    int channel_id,
    const std::string& app_id,
    base::TimeDelta launch_timeout,
    const std::vector<std::string>& supported_app_types,
    const std::optional<base::Value>& app_params,
    LaunchSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    std::move(callback).Run(GetLaunchSessionResponseError(base::StringPrintf(
                                "Socket not found: %d.", channel_id)),
                            nullptr);
    return;
  }
  RecordLaunchSessionChannelFlags(socket->flags());
  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();
  // Cap the max launch timeout to avoid long-living pending requests.
  launch_timeout = std::min(launch_timeout, kLaunchMaxTimeout);
  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", request_id: " << request_id;
  CastMessage message = CreateLaunchRequest(
      source_id_, request_id, app_id, locale_, supported_app_types, app_params);
  if (message.ByteSizeLong() > kMaxCastMessagePayload) {
    std::move(callback).Run(
        GetLaunchSessionResponseError(
            "Message size exceeds maximum cast channel message payload."),
        nullptr);
    return;
  }
  if (requests->AddLaunchRequest(std::make_unique<LaunchSessionRequest>(
                                     request_id, std::move(callback), clock_),
                                 launch_timeout)) {
    SendCastMessageToSocket(socket, message);
  }
}

void CastMessageHandler::StopSession(
    int channel_id,
    const std::string& session_id,
    const std::optional<std::string>& client_id,
    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    std::move(callback).Run(cast_channel::Result::kFailed);
    return;
  }

  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();
  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", request_id: " << request_id;
  if (requests->AddStopRequest(std::make_unique<StopSessionRequest>(
          request_id, std::move(callback), clock_))) {
    SendCastMessageToSocket(
        socket, CreateStopRequest(client_id.value_or(source_id_), request_id,
                                  session_id));
  }
}

Result CastMessageHandler::SendCastMessage(int channel_id,
                                           const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return Result::kFailed;
  }

  SendCastMessageToSocket(socket, message);
  return Result::kOk;
}

Result CastMessageHandler::SendAppMessage(int channel_id,
                                          const CastMessage& message) {
  DCHECK(!IsCastReservedNamespace(message.namespace_()))
      << ": unexpected app message namespace: " << message.namespace_();
  if (message.ByteSizeLong() > kMaxCastMessagePayload) {
    return Result::kFailed;
  }
  return SendCastMessage(channel_id, message);
}

std::optional<int> CastMessageHandler::SendMediaRequest(
    int channel_id,
    const base::Value::Dict& body,
    const std::string& source_id,
    const std::string& destination_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return std::nullopt;
  }

  int request_id = NextRequestId();
  SendCastMessageToSocket(
      socket, CreateMediaRequest(body, request_id, source_id, destination_id));
  return request_id;
}

void CastMessageHandler::SendSetVolumeRequest(int channel_id,
                                              const base::Value::Dict& body,
                                              const std::string& source_id,
                                              ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    std::move(callback).Run(Result::kFailed);
    return;
  }

  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();

  requests->AddVolumeRequest(std::make_unique<SetVolumeRequest>(
      request_id, std::move(callback), clock_));
  SendCastMessageToSocket(socket,
                          CreateSetVolumeRequest(body, request_id, source_id));
}

void CastMessageHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastMessageHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CastMessageHandler::OnError(const CastSocket& socket,
                                 ChannelError error_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int channel_id = socket.id();

  base::EraseIf(virtual_connections_,
                [&channel_id](const VirtualConnection& connection) {
                  return connection.channel_id == channel_id;
                });

  pending_requests_.erase(channel_id);
}

void CastMessageHandler::OnMessage(const CastSocket& socket,
                                   const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1291736): Splitting internal messages into a separate code
  // path with a separate data type is pretty questionable, because it causes
  // duplicated code paths in the downstream logic (manifested as separate
  // OnAppMessage and OnInternalMessage methods).
  if (IsCastReservedNamespace(message.namespace_())) {
    if (message.payload_type() ==
        openscreen::cast::proto::CastMessage_PayloadType_STRING) {
      VLOG(1) << __func__ << ": channel_id: " << socket.id()
              << ", message: " << message;
      parse_json_.Run(
          message.payload_utf8(),
          base::BindOnce(&CastMessageHandler::HandleCastInternalMessage,
                         weak_ptr_factory_.GetWeakPtr(), socket.id(),
                         message.source_id(), message.destination_id(),
                         message.namespace_()));
    } else {
      DLOG(ERROR) << "Dropping internal message with binary payload: "
                  << message.namespace_();
    }
  } else {
    DVLOG(2) << "Got app message from cast channel with namespace: "
             << message.namespace_();
    for (auto& observer : observers_)
      observer.OnAppMessage(socket.id(), message);
  }
}

void CastMessageHandler::OnReadyStateChanged(const CastSocket& socket) {
  if (socket.ready_state() == ReadyState::CLOSED)
    pending_requests_.erase(socket.id());
}

void CastMessageHandler::HandleCastInternalMessage(
    int channel_id,
    const std::string& source_id,
    const std::string& destination_id,
    const std::string& namespace_,
    data_decoder::DataDecoder::ValueOrError parse_result) {
  ASSIGN_OR_RETURN(base::Value value, std::move(parse_result),
                   ReportParseError);

  base::Value::Dict* payload = value.GetIfDict();
  if (!payload) {
    ReportParseError("Parsed message not a dictionary");
    return;
  }

  // Check if the socket still exists as it might have been removed during
  // message parsing.
  if (!socket_service_->GetSocket(channel_id)) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  std::optional<int> request_id = GetRequestIdFromResponse(*payload);
  if (request_id) {
    auto requests_it = pending_requests_.find(channel_id);
    if (requests_it != pending_requests_.end())
      // You might think this method should return in this case, but there is at
      // least one message type (RECEIVER_STATUS), that has a request ID but
      // also needs to be handled by the registered observers.
      requests_it->second->HandlePendingRequest(*request_id, *payload);
  }

  CastMessageType type = ParseMessageTypeFromPayload(*payload);
  if (type == CastMessageType::kOther) {
    DVLOG(2) << "Unknown type in message: " << payload;
    return;
  }

  if (type == CastMessageType::kCloseConnection) {
    // Source / destination is flipped.
    virtual_connections_.erase(
        VirtualConnection(channel_id, destination_id, source_id));
    return;
  }

  InternalMessage internal_message(type, source_id, destination_id, namespace_,
                                   std::move(*payload));
  for (auto& observer : observers_)
    observer.OnInternalMessage(channel_id, internal_message);
}

void CastMessageHandler::SendCastMessageToSocket(CastSocket* socket,
                                                 const CastMessage& message) {
  // A virtual connection must be opened to the receiver before other messages
  // can be sent.
  DoEnsureConnection(socket, message.source_id(), message.destination_id(),
                     GetConnectionType(message.destination_id()));
  VLOG(1) << __func__ << ": channel_id: " << socket->id()
          << ", message: " << message;
  socket->transport()->SendMessage(
      message, base::BindOnce(&CastMessageHandler::OnMessageSent,
                              weak_ptr_factory_.GetWeakPtr()));
}

void CastMessageHandler::DoEnsureConnection(
    CastSocket* socket,
    const std::string& source_id,
    const std::string& destination_id,
    VirtualConnectionType connection_type) {
  VirtualConnection connection(socket->id(), source_id, destination_id);

  // If there is already a connection, there is nothing to do.
  if (virtual_connections_.find(connection) != virtual_connections_.end())
    return;

  VLOG(1) << "Creating VC for channel: " << connection.channel_id
          << ", source: " << connection.source_id
          << ", dest: " << connection.destination_id;
  CastMessage virtual_connection_request = CreateVirtualConnectionRequest(
      connection.source_id, connection.destination_id, connection_type,
      user_agent_, browser_version_);
  socket->transport()->SendMessage(
      virtual_connection_request,
      base::BindOnce(&CastMessageHandler::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr()));

  // We assume the virtual connection request will succeed; otherwise this
  // will eventually self-correct.
  virtual_connections_.insert(connection);
}

void CastMessageHandler::OnMessageSent(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG_IF(2, result < 0) << "SendMessage failed with code: " << result;
}

CastMessageHandler::PendingRequests::PendingRequests() = default;
CastMessageHandler::PendingRequests::~PendingRequests() {
  for (auto& request : pending_app_availability_requests_) {
    std::move(request->callback)
        .Run(request->app_id, GetAppAvailabilityResult::kUnknown);
  }

  if (pending_launch_session_request_) {
    LaunchSessionResponse response;
    response.result = LaunchSessionResponse::kError;
    std::move(pending_launch_session_request_->callback)
        .Run(std::move(response), nullptr);
  }

  if (pending_stop_session_request_)
    std::move(pending_stop_session_request_->callback).Run(Result::kFailed);

  for (auto& request : pending_volume_requests_by_id_)
    std::move(request.second->callback).Run(Result::kFailed);
}

bool CastMessageHandler::PendingRequests::AddAppAvailabilityRequest(
    std::unique_ptr<GetAppAvailabilityRequest> request) {
  const std::string& app_id = request->app_id;
  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::BindOnce(
          &CastMessageHandler::PendingRequests::AppAvailabilityTimedOut,
          base::Unretained(this), request_id));

  // Look for a request with the given app ID.
  bool found = base::Contains(pending_app_availability_requests_, app_id,
                              &GetAppAvailabilityRequest::app_id);
  pending_app_availability_requests_.emplace_back(std::move(request));
  return !found;
}

bool CastMessageHandler::PendingRequests::AddLaunchRequest(
    std::unique_ptr<LaunchSessionRequest> request,
    base::TimeDelta timeout) {
  if (pending_launch_session_request_) {
    std::move(request->callback)
        .Run(cast_channel::GetLaunchSessionResponseError(
                 "There already exists a launch request for the channel"),
             nullptr);
    return false;
  }

  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &CastMessageHandler::PendingRequests::LaunchSessionTimedOut,
          base::Unretained(this), request_id));
  pending_launch_session_request_ = std::move(request);
  return true;
}

bool CastMessageHandler::PendingRequests::AddStopRequest(
    std::unique_ptr<StopSessionRequest> request) {
  if (pending_stop_session_request_) {
    std::move(request->callback).Run(cast_channel::Result::kFailed);
    return false;
  }

  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::BindOnce(&CastMessageHandler::PendingRequests::StopSessionTimedOut,
                     base::Unretained(this), request_id));
  pending_stop_session_request_ = std::move(request);
  return true;
}

void CastMessageHandler::PendingRequests::AddVolumeRequest(
    std::unique_ptr<SetVolumeRequest> request) {
  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::BindOnce(&CastMessageHandler::PendingRequests::SetVolumeTimedOut,
                     base::Unretained(this), request_id));
  pending_volume_requests_by_id_.emplace(request_id, std::move(request));
}

void CastMessageHandler::PendingRequests::HandlePendingRequest(
    int request_id,
    const base::Value::Dict& response) {
  // Look up an app availability request by its |request_id|.
  auto app_availability_it =
      base::ranges::find(pending_app_availability_requests_, request_id,
                         &GetAppAvailabilityRequest::request_id);
  // If we found a request, process and remove all requests with the same
  // |app_id|, which will of course include the one we just found.
  if (app_availability_it != pending_app_availability_requests_.end()) {
    std::string app_id = (*app_availability_it)->app_id;
    GetAppAvailabilityResult result =
        GetAppAvailabilityResultFromResponse(response, app_id);
    std::erase_if(pending_app_availability_requests_,
                  [&app_id, result](const auto& request_ptr) {
                    if (request_ptr->app_id == app_id) {
                      std::move(request_ptr->callback).Run(app_id, result);
                      return true;
                    }
                    return false;
                  });
    return;
  }

  if (pending_launch_session_request_ &&
      pending_launch_session_request_->request_id == request_id) {
    LaunchSessionCallbackWrapper wrapper_callback;

    std::move(pending_launch_session_request_->callback)
        .Run(GetLaunchSessionResponse(response), &wrapper_callback);

    if (wrapper_callback.callback) {
      pending_launch_session_request_->callback =
          std::move(wrapper_callback.callback);
    } else {
      pending_launch_session_request_.reset();
    }

    return;
  }

  if (pending_stop_session_request_ &&
      pending_stop_session_request_->request_id == request_id) {
    std::move(pending_stop_session_request_->callback).Run(Result::kOk);
    pending_stop_session_request_.reset();
    return;
  }

  auto volume_it = pending_volume_requests_by_id_.find(request_id);
  if (volume_it != pending_volume_requests_by_id_.end()) {
    std::move(volume_it->second->callback).Run(Result::kOk);
    pending_volume_requests_by_id_.erase(volume_it);
    return;
  }
}

void CastMessageHandler::PendingRequests::AppAvailabilityTimedOut(
    int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;

  auto it = base::ranges::find(pending_app_availability_requests_, request_id,
                               &GetAppAvailabilityRequest::request_id);

  CHECK(it != pending_app_availability_requests_.end());
  std::move((*it)->callback)
      .Run((*it)->app_id, GetAppAvailabilityResult::kUnknown);
  pending_app_availability_requests_.erase(it);
}

void CastMessageHandler::PendingRequests::LaunchSessionTimedOut(
    int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  CHECK(pending_launch_session_request_);
  CHECK(pending_launch_session_request_->request_id == request_id);

  LaunchSessionResponse response;
  response.result = LaunchSessionResponse::kTimedOut;
  std::move(pending_launch_session_request_->callback)
      .Run(std::move(response), nullptr);
  pending_launch_session_request_.reset();
}

void CastMessageHandler::PendingRequests::StopSessionTimedOut(int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  CHECK(pending_stop_session_request_);
  CHECK(pending_stop_session_request_->request_id == request_id);

  std::move(pending_stop_session_request_->callback).Run(Result::kFailed);
  pending_stop_session_request_.reset();
}

void CastMessageHandler::PendingRequests::SetVolumeTimedOut(int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  auto it = pending_volume_requests_by_id_.find(request_id);
  CHECK(it != pending_volume_requests_by_id_.end(), base::NotFatalUntil::M130);
  std::move(it->second->callback).Run(Result::kFailed);
  pending_volume_requests_by_id_.erase(it);
}

}  // namespace cast_channel
