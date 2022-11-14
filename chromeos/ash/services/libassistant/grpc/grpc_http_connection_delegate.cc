// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_http_connection_delegate.h"
#include "base/task/sequenced_task_runner.h"

namespace ash::libassistant {

using ::assistant::api::StreamHttpConnectionRequest;

GrpcHttpConnectionDelegate::GrpcHttpConnectionDelegate(
    int id,
    GrpcHttpConnectionClient* client)
    : id_(id),
      grpc_http_connection_client_(client),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

GrpcHttpConnectionDelegate::~GrpcHttpConnectionDelegate() = default;

void GrpcHttpConnectionDelegate::OnHeaderResponse(
    const std::string& raw_headers) {
  StreamHttpConnectionRequest request;
  request.set_id(id_);
  request.set_command(StreamHttpConnectionRequest::HANDLE_HEADER_RESPONSE);
  request.set_raw_headers(raw_headers);
  grpc_http_connection_client_->ScheduleRequest(request);
}

void GrpcHttpConnectionDelegate::OnPartialResponse(
    const std::string& partial_response) {
  StreamHttpConnectionRequest request;
  request.set_id(id_);
  request.set_command(StreamHttpConnectionRequest::HANDLE_PARTIAL_RESPONSE);
  request.set_partial_response(partial_response);
  grpc_http_connection_client_->ScheduleRequest(request);
}

void GrpcHttpConnectionDelegate::OnCompleteResponse(
    int http_status,
    const std::string& raw_headers,
    const std::string& response) {
  StreamHttpConnectionRequest request;
  request.set_id(id_);
  request.set_command(StreamHttpConnectionRequest::HANDLE_COMPLETE_RESPONSE);
  auto* res = request.mutable_complete_response();
  res->set_response_code(http_status);
  res->set_raw_headers(raw_headers);
  res->set_response(response);
  grpc_http_connection_client_->ScheduleRequest(request);
}

void GrpcHttpConnectionDelegate::OnNetworkError(int error_code,
                                                const std::string& message) {
  StreamHttpConnectionRequest request;
  request.set_id(id_);
  request.set_command(StreamHttpConnectionRequest::HANDLE_NETWORK_ERROR);
  auto* error = request.mutable_error();
  error->set_error_code(error_code);
  error->set_error_message(message);
  grpc_http_connection_client_->ScheduleRequest(request);
}

void GrpcHttpConnectionDelegate::OnConnectionDestroyed() {
  // Do not inform server to delete the delegate, which is handled by the server
  // side.
  task_runner_->DeleteSoon(FROM_HERE, this);
}

}  // namespace ash::libassistant
