// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_REACTOR_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_REACTOR_H_

#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>

#include <optional>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"

namespace cast {
namespace utils {

// A base class for gRPC server reactors.
template <typename TRequest, typename TResponse>
class GrpcServerReactor : public grpc::ServerGenericBidiReactor {
 public:
  using RequestType = TRequest;

  GrpcServerReactor(const std::string& name,
                    grpc::CallbackServerContext* context)
      : name_(name), context_(context) {}
  ~GrpcServerReactor() override = default;

  // Copy and move are deleted.
  GrpcServerReactor(const GrpcServerReactor&) = delete;
  GrpcServerReactor(GrpcServerReactor&&) = delete;
  GrpcServerReactor& operator=(const GrpcServerReactor&) = delete;
  GrpcServerReactor& operator=(GrpcServerReactor&&) = delete;

  // Flags that the reactor is done (finished or cancelled).
  virtual bool is_done() = 0;

  // Set of overloaded methods to write responses or status to the clients.
  // Writes a status. No writes can be done after this call.
  void Write(const grpc::Status& status) { FinishWriting(nullptr, status); }

  // Writes a defined response.
  void Write(TResponse response = TResponse()) {
    if (response_byte_buffer_) {
      LOG(ERROR) << "Writing is already in progress or reactor is cancelled";
      OnWriteDone(false);
      return;
    }
    DVLOG(1) << "Writing response: " << name();
    response_byte_buffer_.emplace();
    Serialize(response, *response_byte_buffer_);
    // The state machine expects the implementer to write the buffer which will
    // trigger OnWriteDone and reset it back to allow more writes.
    WriteResponse(&*response_byte_buffer_);
  }

  // Returns reactor RPC name.
  const std::string& name() const { return name_; }

 protected:
  // Starts reading a request.
  void ReadRequest() {
    if (request_byte_buffer_) {
      LOG(ERROR) << "Reading is already in progress: " << name();
      OnReadDone(false);
      return;
    }
    DVLOG(1) << "Reading request: " << name();
    request_byte_buffer_.emplace();
    StartRead(&*request_byte_buffer_);
  }

  // The following APIs must be implemented by a certain reactor, and allow
  // proper state tracking while hiding the generic request\response processing.

  // Called when response is written on the wire. An error is set if writes are
  // terminated.
  virtual void WriteResponse(const grpc::ByteBuffer* buffer) = 0;

  // Called to actually write the status on the wire.
  virtual void FinishWriting(const grpc::ByteBuffer* buffer,
                             const grpc::Status& status) = 0;

  // Called to actually write the response serialized into a buffer on the wire.
  virtual void OnResponseDone(const grpc::Status& status) = 0;

  // Called when request was read and deserialized. An error is set if reads are
  // terminated.
  virtual void OnRequestDone(GrpcStatusOr<TRequest> request) = 0;

  // Implements grpc::ServerGenericBidiReactor APIs.
  void OnReadDone(bool ok) override {
    static const grpc::Status kReadsFailedError(grpc::StatusCode::ABORTED,
                                                "Reads failed");
    DVLOG(1) << "Reads done: " << name() << ", ok=" << ok;
    if (!ok) {
      DVLOG(1) << "Reads failed: " << name();
      OnRequestDone(kReadsFailedError);
      return;
    }

    auto request = Deserialize<TRequest>(*request_byte_buffer_);
    request_byte_buffer_.reset();
    OnRequestDone(std::move(request));
  }

  void OnWriteDone(bool ok) override {
    static const grpc::Status kWritesFailedError(grpc::StatusCode::ABORTED,
                                                 "Writes failed");
    DVLOG(1) << "Writes done: " << name() << ", ok=" << ok;
    response_byte_buffer_.reset();
    if (!ok) {
      DVLOG(1) << "Writes failed: " << name();
      OnResponseDone(kWritesFailedError);
      return;
    }

    OnResponseDone(grpc::Status::OK);
  }

  void OnDone() override {
    DVLOG(1) << "Reactor done: " << name();
    delete this;
  }

 private:
  template <typename T>
  void Serialize(T t, grpc::ByteBuffer& buffer) {
    bool own;
    auto status = grpc::SerializationTraits<T>::Serialize(t, &buffer, &own);
    DCHECK(status.ok()) << "Failed to serialize";
  }

  template <typename T>
  T Deserialize(grpc::ByteBuffer& buffer) {
    T t;
    auto status = grpc::SerializationTraits<T>::Deserialize(&buffer, &t);
    DCHECK(status.ok()) << "Failed to serialize";
    return t;
  }

  const std::string name_;
  raw_ptr<grpc::CallbackServerContext> context_;

  std::optional<grpc::ByteBuffer> request_byte_buffer_;
  std::optional<grpc::ByteBuffer> response_byte_buffer_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_REACTOR_H_
