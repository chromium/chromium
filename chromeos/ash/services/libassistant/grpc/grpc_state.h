// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_STATE_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_STATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/assistant/internal/grpc_transport/grpc_client_cq_tag.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"
#include "third_party/grpc/src/include/grpcpp/generic/generic_stub.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/impl/codegen/client_context.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace ash::libassistant {

// Configs which dictate options for an RPCState instance.
struct StateConfig {
  StateConfig() = default;
  ~StateConfig() = default;
  StateConfig(int32_t retries, int64_t timeout_in_ms)
      : max_retries(retries), timeout_in_ms(timeout_in_ms) {}

  // The maximum retry attempts for the client call if it failed.
  int32_t max_retries = 0;

  // Deadline for the client call.
  int64_t timeout_in_ms = 2000;

  // If set to true, the RPC will be queued and not "fail fast" if the channel
  // is in TRANSIENT_FAILURE or CONNECTING state, and wait until the channel
  // turns READY. Otherwise, such gRPCs will be failed immediately.
  bool wait_for_ready = true;
};

// Object allocated per active RPC.
// Manage the state of a single asynchronous RPC request. If `max_retries`
// is greater than 0, the request will be retried for any transient failures
// as long as the overall deadline has not elapsed.
template <class Response>
class RPCState : public chromeos::libassistant::GrpcClientCQTag {
 public:
  // Async RPCState constructor.
  // Default behavior is to set wait_for_ready = true and handle timeouts
  // manually.
  RPCState(std::shared_ptr<grpc::Channel> channel,
           grpc::CompletionQueue* cq,
           const grpc::string& method,
           const google::protobuf::MessageLite& request,
           ResponseCallback<grpc::Status, Response> done,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           StateConfig config)
      : async_cb_(std::move(done)),
        callback_task_runner_(callback_task_runner),
        cq_(cq),
        stub_(channel),
        method_(method),
        timeout_in_ms_(config.timeout_in_ms),
        max_retries_(config.max_retries),
        wait_for_ready_(config.wait_for_ready) {
    DCHECK(cq);
    DCHECK(callback_task_runner);

    grpc::Status s = GrpcSerializeProto(request, &request_buf_);
    if (!s.ok()) {
      LOG(ERROR) << "GrpcSerializeProto returned with non-ok status: "
                 << s.error_message();
      // Skip retry logic if we fail to parse our request.
      StateDone();
      return;
    }

    StartCall();
  }

  RPCState(const RPCState&) = delete;
  RPCState& operator=(const RPCState&) = delete;
  ~RPCState() override = default;

  void StartCall() {
    context_ = std::make_unique<grpc::ClientContext>();
    context_->set_wait_for_ready(wait_for_ready_);

    if (timeout_in_ms_ > 0) {
      context_->set_deadline(
          gpr_time_from_millis(timeout_in_ms_, GPR_TIMESPAN));
    }

    VLOG(3) << "Starting call: " << method_;
    call_ = stub_.PrepareUnaryCall(context_.get(), method_, request_buf_, cq_);
    call_->StartCall();
    // Request that upon the completion of an RPC call, |response_buf_| will be
    // updated with server's response. Tag the call with |this| to identify this
    // request.
    call_->Finish(&response_buf_, &status_, /*tag=*/this);
  }

  // GrpcClientCQTag overrides:
  // Invoked from the completion queue thread.
  void OnCompleted(State state) override {
    VLOG(3) << "Completed call: " << method_;

    if (state == State::kShutdown) {
      LOG(WARNING) << "Unary RPC done with CQ has been shutting down.";
      ParseAndCallDone();
      return;
    }

    if (status_.ok() || status_.error_code() == grpc::StatusCode::CANCELLED) {
      ParseAndCallDone();
      return;
    }

    LOG_IF(WARNING, ShouldLogGrpcError())
        << method_ << " returned with non-ok status: " << status_.error_code()
        << " Retries: " << num_retries_ << " Max: " << max_retries_ << "\n";
    // TODO(nanping): Retry only for logical errors by having them in the
    // config.
    // Retry if we have any attempts left
    if (num_retries_ < max_retries_) {
      ++num_retries_;
      response_buf_.Clear();
      LOG_IF(WARNING, ShouldLogGrpcError())
          << "Retrying call for " << method_ << "Retry: " << num_retries_
          << " of " << max_retries_;
      StartCall();
    } else {
      // Attach additional GRPC error information if any to the final status
      LOG_IF(ERROR, ShouldLogGrpcError()) << "RPC call failed :\n";
      StateDone();
    }
  }

  // Runs on the completion queue thread.
  void ParseAndCallDone() {
    if (!GrpcParseProto(&response_buf_, &async_response_)) {
      LOG(ERROR) << "RPC parse response failed.";
    }
    StateDone();
  }

  // Run on the completion queue thread.
  void StateDone() {
    DCHECK(async_cb_);
    // |async_cb_| must be invoked from its original sequence.
    callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(async_cb_), status_, async_response_));

    delete this;
  }

 private:
  bool ShouldLogGrpcError() {
    // Some grpc errors are legitimate/expected. Ex: ReadSecureFile() may return
    // NOT_FOUND if the file doesn't exist. Do not log warning/errors since it's
    // just spam. The caller can log the error if desired.
    return status_.error_code() != grpc::StatusCode::NOT_FOUND;
  }

  // An instance managing the context settings, e.g. deadline, relevant to the
  // call they are invoked with. Same object should not be reused across RPCs.
  std::unique_ptr<::grpc::ClientContext> context_;

  // Message response of type |Response| received from the server.
  Response async_response_;

  // Buffer filled in with request/response.
  grpc::ByteBuffer request_buf_;
  grpc::ByteBuffer response_buf_;

  // Status of a RPC call. The status is OK if the call finished with no errors.
  grpc::Status status_;

  // |async_cb_| must always be called from the main thread.
  ResponseCallback<grpc::Status, Response> async_cb_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // An instance used by an async gRPC client to manage asynchronous rpc
  // operations. An RPC call is bound to a CompletionQueue when performed
  // using the stub.
  raw_ptr<grpc::CompletionQueue> cq_ = nullptr;

  // An instance used by a gRPC client to invoke rpc methods implemented in
  // the server.
  grpc::GenericStub stub_;

  // An instance held a unary RPC call and exposes methods to start and finish
  // the call with server's response.
  std::unique_ptr<::grpc::GenericClientAsyncResponseReader> call_;

  // The name of a RPC method.
  grpc::string method_;

  // Config options for a RPC call.
  int64_t timeout_in_ms_;
  size_t max_retries_;
  bool wait_for_ready_;
  size_t num_retries_ = 0;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_STATE_H_
