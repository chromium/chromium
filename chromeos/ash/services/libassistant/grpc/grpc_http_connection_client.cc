// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_http_connection_client.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_http_connection_delegate.h"
#include "chromeos/assistant/internal/grpc_transport/streaming/bidi_streaming_rpc_call.h"
#include "chromeos/assistant/internal/grpc_transport/streaming/streaming_write_queue.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpc/impl/codegen/grpc_types.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/support/channel_arguments.h"

namespace ash::libassistant {

namespace {
using ::assistant::api::StreamHttpConnectionRequest;
using ::assistant::api::StreamHttpConnectionResponse;
using assistant_client::HttpConnection;
using ::chromeos::libassistant::BidiStreamingRpcCall;
using ::chromeos::libassistant::StreamingWriteQueue;
using ::chromeos::libassistant::StreamingWriter;

HttpConnection::Method ConvertToHttpConnectionMethod(
    StreamHttpConnectionResponse::Method method) {
  switch (method) {
    case StreamHttpConnectionResponse::GET:
      return HttpConnection::GET;
    case StreamHttpConnectionResponse::POST:
      return HttpConnection::POST;
    case StreamHttpConnectionResponse::HEAD:
      return HttpConnection::HEAD;
    case StreamHttpConnectionResponse::PATCH:
      return HttpConnection::PATCH;
    case StreamHttpConnectionResponse::PUT:
      return HttpConnection::PUT;
    case StreamHttpConnectionResponse::DELETE:
      return HttpConnection::DELETE;
    case StreamHttpConnectionResponse::METHOD_UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return HttpConnection::GET;
  }
}

// A macro which ensures we are running on the calling sequence.
#define ENSURE_CALLING_SEQUENCE(method, ...)                                \
  DVLOG(3) << __func__;                                                     \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                        \
    task_runner_->PostTask(                                                 \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

}  // namespace

GrpcHttpConnectionClient::GrpcHttpConnectionClient(
    assistant_client::HttpConnectionFactory* http_connection_factory,
    const std::string& server_address)
    : http_connection_factory_(http_connection_factory),
      cq_thread_(std::make_unique<GrpcClientThread>("http_connection_cq")),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Make sure to turn off compression.
  grpc::ChannelArguments channel_args;
  channel_args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 2000);
  channel_args.SetCompressionAlgorithm(
      grpc_compression_algorithm::GRPC_COMPRESS_NONE);
  grpc_local_connect_type connect_type =
      GetGrpcLocalConnectType(server_address);
  channel_ = grpc::CreateCustomChannel(
      server_address, grpc::experimental::LocalCredentials(connect_type),
      channel_args);
  stub_ = ::assistant::api::HttpConnectionService::NewStub(channel_);
}

GrpcHttpConnectionClient::~GrpcHttpConnectionClient() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  CleanUp();

  {
    base::AutoLock lock(write_queue_lock_);
    is_shutting_down_ = true;
  }

  if (call_) {
    {
      base::AutoLock lock(write_queue_lock_);
      write_queue_.reset();
    }

    call_->TryCancel();
    cq_thread_.reset();
  }
}

void GrpcHttpConnectionClient::Start() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (call_) {
    {
      base::AutoLock lock(write_queue_lock_);
      write_queue_.reset();
    }

    call_->TryCancel();
    call_.reset();
  }

  {
    base::AutoLock lock(write_queue_lock_);
    write_queue_ =
        std::make_unique<StreamingWriteQueue<StreamHttpConnectionRequest>>();
  }

  // Create a bidi streaming call to relay http connection from Libassistant.
  BidiStreamingRpcCall<StreamHttpConnectionRequest,
                       StreamHttpConnectionResponse>::CallbackParams cb_params;
  cb_params.write_available_cb = base::BindRepeating(
      &GrpcHttpConnectionClient::OnRpcWriteAvailable, base::Unretained(this));
  cb_params.read_available_cb = base::BindRepeating(
      &GrpcHttpConnectionClient::OnRpcReadAvailable, base::Unretained(this));
  cb_params.exited_cb = base::BindRepeating(
      &GrpcHttpConnectionClient::OnRpcExited, base::Unretained(this));
  call_ = std::make_unique<BidiStreamingRpcCall<StreamHttpConnectionRequest,
                                                StreamHttpConnectionResponse>>(
      std::move(cb_params));
  auto stream = stub_->PrepareAsyncStreamHttpConnection(
      call_->ctx(), cq_thread_->completion_queue());
  call_->Start(std::move(stream));
}

void GrpcHttpConnectionClient::CleanUp() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case |http_connections_| is non-empty, make sure `Close()` is called.
  for (auto iter = http_connections_.begin(); iter != http_connections_.end();
       ++iter) {
    iter->second->Close();
  }
  http_connections_.clear();
  delegates_.clear();
}

void GrpcHttpConnectionClient::ScheduleRequest(
    StreamHttpConnectionRequest request) {
  base::AutoLock lock(write_queue_lock_);
  if (is_shutting_down_) {
    return;
  }

  if (write_queue_) {
    write_queue_->ScheduleWrite(std::move(request));
  }
}

// Called when the RPC channel is idle and ready to accept new write.
void GrpcHttpConnectionClient::OnRpcWriteAvailable(
    grpc::ClientContext* context,
    StreamingWriter<StreamHttpConnectionRequest>* writer) {
  {
    base::AutoLock lock(write_queue_lock_);
    if (is_shutting_down_) {
      return;
    }
  }

  if (!init_request_sent_) {
    DVLOG(1) << "Sending GrpcHttpConnectionClient registration request.";
    init_request_sent_ = true;
    // Send initial request to signal readiness for streaming.
    StreamHttpConnectionRequest request;
    request.set_command(StreamHttpConnectionRequest::REGISTER);
    writer->Write(std::move(request));
    return;
  }

  {
    base::AutoLock lock(write_queue_lock_);

    if (write_queue_) {
      write_queue_->OnRpcWriteAvailable(writer);
    }
  }
}

void GrpcHttpConnectionClient::OnRpcReadAvailable(
    grpc::ClientContext* context,
    const StreamHttpConnectionResponse& response) {
  ENSURE_CALLING_SEQUENCE(&GrpcHttpConnectionClient::OnRpcReadAvailable,
                          context, response);

  DCHECK(response.has_id());
  const int http_connection_id = response.id();
  const auto iter = http_connections_.find(http_connection_id);
  if (iter == http_connections_.end() &&
      response.command() != StreamHttpConnectionResponse::CREATE) {
    DVLOG(2) << "Ignoring the HttpConnection request because the http "
                "connection does not exist.";
    return;
  }

  switch (response.command()) {
    case StreamHttpConnectionResponse::CREATE: {
      DVLOG(1) << "StreamHttpConnectionResponse::CREATE";
      if (iter != http_connections_.end()) {
        LOG(ERROR) << "Failed to create the http connection because of "
                      "duplicated id: "
                   << http_connection_id;
        return;
      }
      {
        DVLOG(1) << "Ceate the http connection " << http_connection_id;
        auto* delegate =
            new GrpcHttpConnectionDelegate(http_connection_id, this);
        auto* http_connection = http_connection_factory_->Create(delegate);
        http_connections_.insert({http_connection_id, http_connection});
        delegates_.insert({http_connection_id, delegate});
      }
      break;
    }
    case StreamHttpConnectionResponse::START: {
      DVLOG(1) << "StreamHttpConnectionResponse::START";
      DCHECK(response.has_parameters());
      const auto& param = response.parameters();
      auto* http_connection = iter->second.get();
      http_connection->SetRequest(
          param.url(), ConvertToHttpConnectionMethod(param.method()));
      for (const auto& header : param.headers()) {
        http_connection->AddHeader(header.name(), header.value());
      }
      if (!param.upload_content_type().empty()) {
        DCHECK(param.chunked_upload_content_type().empty());
        http_connection->SetUploadContent(param.upload_content(),
                                          param.upload_content_type());
      } else if (!param.chunked_upload_content_type().empty()) {
        DCHECK(param.upload_content_type().empty());
        http_connection->SetChunkedUploadContentType(
            param.chunked_upload_content_type());
      }
      if (param.enable_header_response()) {
        http_connection->EnableHeaderResponse();
      }
      if (param.enable_partial_response()) {
        http_connection->EnablePartialResults();
      }
      http_connection->Start();
      break;
    }
    case StreamHttpConnectionResponse::PAUSE:
      DVLOG(1) << "StreamHttpConnectionResponse::PAUSE";
      iter->second->Pause();
      break;
    case StreamHttpConnectionResponse::RESUME:
      DVLOG(1) << "StreamHttpConnectionResponse::RESUME";
      iter->second->Resume();
      break;
    case StreamHttpConnectionResponse::CLOSE: {
      DVLOG(1) << "StreamHttpConnectionResponse::CLOSE";
      iter->second->Close();
      http_connections_.erase(iter);

      const auto delegate_iter = delegates_.find(http_connection_id);
      DCHECK(delegate_iter != delegates_.end());
      delegates_.erase(delegate_iter);
      break;
    }
    case StreamHttpConnectionResponse::UPLOAD_DATA:
      DVLOG(1) << "StreamHttpConnectionResponse::UPLOAD_DATA";
      iter->second->UploadData(response.chunked_data().data(),
                               response.chunked_data().is_last_chunk());
      break;
    case StreamHttpConnectionResponse::COMMAND_UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
  }
}

void GrpcHttpConnectionClient::OnRpcExited(grpc::ClientContext* context,
                                           const grpc::Status& status) {
  ENSURE_CALLING_SEQUENCE(&GrpcHttpConnectionClient::OnRpcExited, context,
                          status);
  DVLOG(1) << "GrpcHttpConnectionClient streaming exited with status "
           << (status.ok() ? "ok" : status.error_message());
  init_request_sent_ = false;
  // If the streaming session failed unexpectedly. Since client (this class) is
  // the one who initiates the streaming connection, it's the only one who can
  // repair a broken session. The server (Libassistant) is helpless in this
  // case, so it's important that the client diligently maintains a healthy
  // connection.
  if (!status.ok()) {
    DVLOG(2) << "Retry to establish GrpcHttpConnection streaming session.";
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GrpcHttpConnectionClient::Start,
                                          weak_factory_.GetWeakPtr()));
  } else {
    DVLOG(1) << "GrpcHttpConnection exited.";
  }

  CleanUp();
}

}  // namespace ash::libassistant
