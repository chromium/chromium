// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/legion/proto/legion.pb.h"
#include "components/legion/proto_utils/generate_content_response_utils.h"

namespace legion {

namespace {

void OnGenerateContentRequestCompleted(
    Client::OnTextRequestCompletedCallback cb,
    base::expected<proto::GenerateContentResponse, ErrorCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  auto text = ConvertGenerateContentResponseToText(*result);
  if (!text.has_value()) {
    LOG(ERROR) << "GenerateContentResponse did not contain any content";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoContent));
    return;
  }

  std::move(cb).Run(text.value());
}

void OnRequestSent(
    Client::OnGenerateContentRequestCompletedCallback cb,
    base::expected<ClientImpl::BinaryEncodedProtoResponse, ErrorCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  proto::LegionResponse legion_response;
  if (!legion_response.ParseFromArray(result->data(), result->size())) {
    LOG(ERROR) << "Failed to parse LegionResponse";
    std::move(cb).Run(base::unexpected(ErrorCode::kResponseParseError));
    return;
  }

  if (!legion_response.has_generate_content_response()) {
    LOG(ERROR) << "LegionResponse did not contain a "
                  "generate_content_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }

  std::move(cb).Run(legion_response.generate_content_response());
}

}  // namespace

ClientImpl::ClientImpl(SecureChannelFactory channel_factory)
    : secure_channel_factory_(std::move(channel_factory)) {}

ClientImpl::~ClientImpl() = default;

void ClientImpl::EstablishSession(
    OnEstablishSessionCompletedCallback callback) {
  GetOrCreateSecureChannel()->EstablishChannel(
      base::BindOnce(&ClientImpl::OnSessionEstablished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

SecureChannel* ClientImpl::GetOrCreateSecureChannel() {
  if (!secure_channel_) {
    secure_channel_ = secure_channel_factory_.Run();
    secure_channel_->SetResponseCallback(base::BindRepeating(
        &ClientImpl::OnResponseReceived, base::Unretained(this)));
  }
  return secure_channel_.get();
}

void ClientImpl::SendRequest(int32_t request_id,
                             BinaryEncodedProtoRequest request,
                             OnRequestCompletedCallback callback,
                             base::TimeDelta timeout) {
  DVLOG(1) << "SendRequest started.";

  // Records the request size in bytes. The max value is 1M bytes.
  base::UmaHistogramCounts1M("Legion.Client.RequestSize", request.size());
  auto wrapped_callback = base::BindOnce(
      &ClientImpl::OnRequestCompleted, weak_factory_.GetWeakPtr(),
      std::move(callback), base::TimeTicks::Now());

  if (GetOrCreateSecureChannel()->Write(std::move(request))) {
    pending_requests_.emplace(request_id, std::move(wrapped_callback));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ClientImpl::OnRequestTimeout,
                       weak_factory_.GetWeakPtr(), request_id),
        timeout);
  } else {
    // The channel is in a permanent failure state, so fail the current request.
    DVLOG(1) << "Secure channel write failed.";
    std::move(wrapped_callback).Run(base::unexpected(ErrorCode::kError));
  }
}

void ClientImpl::SendTextRequest(proto::FeatureName feature_name,
                                 const std::string& text,
                                 OnTextRequestCompletedCallback callback,
                                 const RequestOptions& options) {
  proto::GenerateContentRequest request;
  if (feature_name ==
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT) {
    request.set_model("dev_v3xs");
  }
  auto* content = request.add_contents();
  content->set_role("user");
  auto* part = content->add_parts();
  part->set_text(text);

  auto text_response_callback =
      base::BindOnce(&OnGenerateContentRequestCompleted, std::move(callback));

  SendGenerateContentRequest(feature_name, request,
                             std::move(text_response_callback), options);
}

void ClientImpl::SendGenerateContentRequest(
    proto::FeatureName feature_name,
    const proto::GenerateContentRequest& request,
    OnGenerateContentRequestCompletedCallback callback,
    const RequestOptions& options) {
  int32_t request_id = next_request_id_;
  next_request_id_++;

  proto::LegionRequest request_proto;
  request_proto.set_feature_name(feature_name);
  request_proto.set_request_id(request_id);
  *request_proto.mutable_generate_content_request() = request;

  base::UmaHistogramSparse("Legion.Client.FeatureName",
                           static_cast<int>(feature_name));

  std::string serialized_request;
  request_proto.SerializeToString(&serialized_request);
  BinaryEncodedProtoRequest binary_encoded_proto_request(
      serialized_request.begin(), serialized_request.end());

  // The callback for when the response is received.
  auto response_parsing_callback =
      base::BindOnce(&OnRequestSent, std::move(callback));

  SendRequest(request_id, std::move(binary_encoded_proto_request),
              std::move(response_parsing_callback), options.timeout);
}

void ClientImpl::FailAllPendingRequests(ErrorCode error_code) {
  auto pending_requests = std::move(pending_requests_);
  for (auto& entry : pending_requests) {
    std::move(entry.second).Run(base::unexpected(error_code));
  }
}

void ClientImpl::OnSessionEstablished(
    OnEstablishSessionCompletedCallback callback,
    base::expected<void, ErrorCode> result) {
  std::move(callback).Run(std::move(result));
}

void ClientImpl::OnRequestTimeout(int32_t request_id) {
  auto it = pending_requests_.find(request_id);
  if (it != pending_requests_.end()) {
    DLOG(ERROR) << "Request timed out: " << request_id;
    timed_out_requests_.insert(request_id);
    auto callback = std::move(it->second);
    pending_requests_.erase(it);
    std::move(callback).Run(base::unexpected(ErrorCode::kTimeout));
  }
}

void ClientImpl::OnResponseReceived(
    base::expected<BinaryEncodedProtoResponse, ErrorCode> result) {
  if (!result.has_value()) {
    // The secure channel is broken. Fail all pending requests and destroy the
    // channel. It will be recreated on the next request.
    DVLOG(1) << "Secure channel read failed. Destroying channel.";
    FailAllPendingRequests(result.error());
    secure_channel_.reset();
    return;
  }

  proto::LegionResponse legion_response;
  if (!legion_response.ParseFromArray(result->data(), result->size())) {
    LOG(ERROR) << "Failed to parse LegionResponse";
    // This is a protocol error. We don't know which request this response was
    // for, so we fail all of them.
    FailAllPendingRequests(ErrorCode::kResponseParseError);
    return;
  }

  auto it = pending_requests_.find(legion_response.request_id());
  if (it == pending_requests_.end()) {
    auto timed_out_it = timed_out_requests_.find(legion_response.request_id());
    if (timed_out_it != timed_out_requests_.end()) {
      DLOG(ERROR) << "Received response for timed out request_id: "
                  << legion_response.request_id();
      timed_out_requests_.erase(timed_out_it);
    } else {
      DLOG(ERROR) << "Received response for unknown request_id: "
                  << legion_response.request_id();
    }
    // This could be a response to a request that has already timed out and was
    // removed from the pending list. In this case we should just ignore it and
    // not cancel other pending requests.
    return;
  }

  auto callback = std::move(it->second);
  pending_requests_.erase(it);

  std::move(callback).Run(std::move(result));
}

void ClientImpl::OnRequestCompleted(
    OnRequestCompletedCallback callback,
    base::TimeTicks start_time,
    base::expected<BinaryEncodedProtoResponse, ErrorCode> result) {
  const auto latency = base::TimeTicks::Now() - start_time;

  if (result.has_value()) {
    // Records the response size in bytes. The max value is 1M bytes.
    base::UmaHistogramCounts1M("Legion.Client.ResponseSize.Success",
                               result->size());
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Success",
                                  latency);
  } else if (result.error() == ErrorCode::kTimeout) {
    base::UmaHistogramEnumeration("Legion.Client.RequestErrorCode",
                                  ErrorCode::kTimeout);
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Timeout",
                                  latency);
  } else {
    base::UmaHistogramEnumeration("Legion.Client.RequestErrorCode",
                                  result.error());
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Error",
                                  latency);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace legion
