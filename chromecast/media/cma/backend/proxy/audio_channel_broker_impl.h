// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_BROKER_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_BROKER_IMPL_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.grpc.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.pb.h"
#include "third_party/grpc/src/include/grpcpp/impl/codegen/status.h"
#include "third_party/protobuf/src/google/protobuf/duration.pb.h"

namespace chromecast {

class TaskRunner;

namespace media {

// This class provides an implementation of the CastRuntimeAudioChannelBroker
// interface using the public gRPC library's asynchronous APIs.
class AudioChannelBrokerImpl
    : public CastRuntimeAudioChannelBroker,
      public cast::media::CastAudioChannelService::CallbackService {
 public:
  // |task_runner| and |handler| must remain valid for the lifetime of this
  // object.
  AudioChannelBrokerImpl(TaskRunner* task_runner, Handler* handler);
  AudioChannelBrokerImpl(const AudioChannelBrokerImpl& other) = delete;
  AudioChannelBrokerImpl(AudioChannelBrokerImpl&& other) = delete;
  ~AudioChannelBrokerImpl() override;

  AudioChannelBrokerImpl& operator=(const AudioChannelBrokerImpl& other) =
      delete;
  AudioChannelBrokerImpl& operator=(AudioChannelBrokerImpl&& other) = delete;

  // CastRuntimeAudioChannelBroker overrides:
  //
  // Note that all below methods may be called from any thread.
  void InitializeAsync(const std::string& cast_session_id,
                       CastAudioDecoderMode decoder_mode) override;
  void SetVolumeAsync(float multiplier) override;
  void SetPlaybackAsync(double playback_rate) override;
  void GetMediaTimeAsync() override;
  void StartAsync(int64_t pts_micros, TimestampInfo timestamp_info) override;
  void StopAsync() override;
  void PauseAsync() override;
  void ResumeAsync(TimestampInfo timestamp_info) override;
  void UpdateTimestampAsync(TimestampInfo timestamp_info) override;

 private:
  using GrpcStub = cast::media::CastAudioChannelService::StubInterface;

  // Helper to abstract away the complexity of using gRPC Calls with the new
  // gRPC async APIs.
  //
  // Handles creating a "Request" type which is then populated with call
  // parameters, and an async call is sent with CallAsync(). This calls into
  // the user-specified callback.
  template <typename TParams, typename TResult>
  struct GrpcCall {
    // The response to a gRPC Call.
    class Response : public grpc::ClientUnaryReactor {
     public:
      using Callback =
          base::OnceCallback<void(CastRuntimeAudioChannelBroker::StatusCode)>;

      Response(std::unique_ptr<TParams> params)
          : parameters_(std::move(params)),
            response_(std::make_unique<TResult>()),
            context_(std::make_unique<grpc::ClientContext>()) {}

      ~Response() override = default;

      // Sets the callback to be called upon completion of the associated gRPC
      // call. Must be set before this occurs.
      void SetCallback(Callback callback) { callback_ = std::move(callback); }

      TParams& parameters() const { return *parameters_; }
      TResult& response() const { return *response_; }
      grpc::ClientContext& context() const { return *context_; }

     private:
      // grpc::ClientUnaryReactor overrides:
      //
      // Per documentation, "If it is never called on an RPC, it indicates an
      // application-level problem (like failure to remove a hold)." So this
      // method should always be called, and the |callback_| will delete this
      // instance
      void OnDone(const grpc::Status& s) override {
        std::move(callback_).Run(GetStatusCode(s));
      }

      static inline CastRuntimeAudioChannelBroker::StatusCode GetStatusCode(
          const grpc::Status& status) {
        return static_cast<CastRuntimeAudioChannelBroker::StatusCode>(
            status.error_code());
      }

      std::unique_ptr<TParams> parameters_;
      std::unique_ptr<TResult> response_;
      std::unique_ptr<grpc::ClientContext> context_;
      Callback callback_;
    };

    // Handles populating and sending a gRPC Request.
    class Request {
     public:
      using StubCallMethod =
          base::OnceCallback<void(grpc::ClientContext*,
                                  const TParams*,
                                  TResult*,
                                  grpc::ClientUnaryReactor*)>;
      using ResponseCallback =
          base::OnceCallback<void(std::unique_ptr<Response>,
                                  CastRuntimeAudioChannelBroker::StatusCode)>;

      Request(StubCallMethod stub_call, ResponseCallback response_callback)
          : stub_call_(std::move(stub_call)),
            response_callback_(std::move(response_callback)),
            parameters_(std::make_unique<TParams>()) {}

      void CallAsync() {
        // Create a new Response.
        std::unique_ptr<Response> response =
            std::make_unique<Response>(std::move(parameters_));
        Response* response_ptr = response.get();

        // Transfer ownership of the response into a callback.
        auto callback =
            base::BindOnce(std::move(response_callback_), std::move(response));

        // Transfer ownership of that callback to the the response object it now
        // owns.
        response_ptr->SetCallback(std::move(callback));

        // Make the gRPC Call. The unique_ptr containing the transferred
        // |response| instance will be moved to the callback method, and then
        // deleted when it goes out of scope.
        std::move(stub_call_)
            .Run(&response_ptr->context(), &response_ptr->parameters(),
                 &response_ptr->response(), response_ptr);
      }

      TParams& parameters() const { return *parameters_; }

     private:
      StubCallMethod stub_call_;
      ResponseCallback response_callback_;
      std::unique_ptr<TParams> parameters_;
    };

    // Helper for creating a gRPC Request using only the remote call and
    // callback to be called.
    //
    // |async_call| is a member-function pointer to the gRPC Stub method to call
    // asynchronously.
    // |callback| is a member function pointer to the callback to use when a
    // response occurs. This will ALWAYS be called, even on a failed or
    // canceled gRPC call.
    template <typename CallStub, typename CallbackClass>
    static Request CreateRequest(
        AudioChannelBrokerImpl* instance,
        void (CallStub::*async_call)(grpc::ClientContext*,
                                     const TParams*,
                                     TResult*,
                                     grpc::ClientUnaryReactor*),
        void (CallbackClass::*callback)(
            std::unique_ptr<Response>,
            CastRuntimeAudioChannelBroker::StatusCode)) {
      auto bound_async_call = base::BindOnce(
          async_call,
          base::Unretained(instance->stub_factory_.GetStub()->async()));
      auto bound_callback =
          base::BindOnce(callback, instance->weak_factory_.GetWeakPtr());
      return Request(std::move(bound_async_call), std::move(bound_callback));
    }
  };

  // Helper to initialize the service stub at runtime instead of instance
  // creation time.
  class LazyStubFactory {
   public:
    using Stub = cast::media::CastAudioChannelService::StubInterface;

    LazyStubFactory();
    ~LazyStubFactory();

    Stub* GetStub();

   private:
    std::unique_ptr<Stub> stub_;
  };

  // Some aliases to make the code more readable.
  using InitializeCall =
      GrpcCall<cast::media::InitializeRequest, cast::media::InitializeResponse>;
  using StateChangeCall = GrpcCall<cast::media::StateChangeRequest,
                                   cast::media::StateChangeResponse>;
  using SetVolumeCall =
      GrpcCall<cast::media::SetVolumeRequest, cast::media::SetVolumeResponse>;
  using SetPlaybackCall = GrpcCall<cast::media::SetPlaybackRateRequest,
                                   cast::media::SetPlaybackRateResponse>;
  using GetMediaTimeCall = GrpcCall<cast::media::GetMediaTimeRequest,
                                    cast::media::GetMediaTimeResponse>;
  using PushBufferCall =
      GrpcCall<cast::media::PushBufferRequest, cast::media::PushBufferResponse>;

  // Helper to create the call types used more than once.
  StateChangeCall::Request CreateStateChangeCallRequest();

  // Calls PushBuffer if data to push is available. Else, schedule this task to
  // run again in the future.
  void TryPushBuffer();

  // Callbacks for asynchronous gRPC calls. Each calls into the appropriate
  // |handler_| method.
  void OnGetMediaTime(std::unique_ptr<GetMediaTimeCall::Response> call_details,
                      CastRuntimeAudioChannelBroker::StatusCode status_code);
  void OnSetPlayback(std::unique_ptr<SetPlaybackCall::Response> call_details,
                     CastRuntimeAudioChannelBroker::StatusCode status_code);
  void OnSetVolume(std::unique_ptr<SetVolumeCall::Response> call_details,
                   CastRuntimeAudioChannelBroker::StatusCode status_code);
  void OnStateChange(std::unique_ptr<StateChangeCall::Response> call_details,
                     CastRuntimeAudioChannelBroker::StatusCode status_code);
  void OnInitialize(std::unique_ptr<InitializeCall::Response> call_details,
                    CastRuntimeAudioChannelBroker::StatusCode status_code);

  // Callback for PushBuffer asynchronous gRPC call. Calls into
  // |handler_->HandlePushBufferResponse()| then tries to push more data.
  void OnPushBuffer(std::unique_ptr<PushBufferCall::Response> call_details,
                    CastRuntimeAudioChannelBroker::StatusCode status_code);

  LazyStubFactory stub_factory_;

  Handler* const handler_;
  TaskRunner* const task_runner_;

  base::WeakPtrFactory<AudioChannelBrokerImpl> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_BROKER_IMPL_H_
