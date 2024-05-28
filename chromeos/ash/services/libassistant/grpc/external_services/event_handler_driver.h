// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_EVENT_HANDLER_DRIVER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_EVENT_HANDLER_DRIVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/services/libassistant/grpc/async_service_driver.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/rpc_method_driver.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_service.grpc.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/event_notification_interface.pb.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace ash::libassistant {

// Create request to register event handler, setting fields accordingly. It
// cannot be a virtual method because it will be used in constructor. Derived
// class should implement specialized function.
template <typename THandlerInterface>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest(
    const std::string& assistant_service_address_);

// EventHandlerDriver is template base class of each libassistant gRPC
// event handler. There will be one instance for each event type. Whoever wants
// to observe libassistant gRPC event should add themselves as observers.
template <typename THandlerInterface>
class EventHandlerDriver : public AsyncServiceDriver {
 public:
  template <typename Func>
  struct UnwrapTypeFromInterface;

  template <typename TRequest, typename TResponse, typename Scope>
  struct UnwrapTypeFromInterface<::grpc::Status (  // NOLINT(whitespace/parens)
      Scope::*)(grpc::ServerContext*, const TRequest*, TResponse*)> {
   private:
    typedef TRequest RequestType;
    typedef TResponse ResponseType;

    friend class EventHandlerDriver;
  };

  typedef typename UnwrapTypeFromInterface<
      decltype(&THandlerInterface::AsyncService::OnEventFromLibas)>::
      ResponseType ResponseType;
  typedef typename UnwrapTypeFromInterface<
      decltype(&THandlerInterface::AsyncService::OnEventFromLibas)>::RequestType
      RequestType;
  using EventObserverType = GrpcServicesObserver<RequestType>;

  EventHandlerDriver(::grpc::ServerBuilder* server_builder,
                     GrpcLibassistantClient* libassistant_client,
                     const std::string& assistant_service_address)
      : AsyncServiceDriver(server_builder),
        libassistant_client_(libassistant_client),
        assistant_service_address_(assistant_service_address) {
    DCHECK(server_builder);
    DCHECK(libassistant_client_);

    server_builder_->RegisterService(&service_);
  }

  ~EventHandlerDriver() override = default;

  void StartRegistration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Make request to libassistant, registering handler.
    ::assistant::api::RegisterEventHandlerRequest request =
        CreateRegistrationRequest<THandlerInterface>(
            assistant_service_address_);
    StateConfig config;
    config.max_retries = 5;
    config.timeout_in_ms = 3000;
    libassistant_client_->CallServiceMethod(
        request,
        base::BindOnce(&EventHandlerDriver::OnRegisterEventHandlerDone,
                       weak_factory_.GetWeakPtr()),
        std::move(config));
  }

  void AddObserver(EventObserverType* const observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(EventObserverType* const observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  void HandleEvent(
      grpc::ServerContext* context,
      const RequestType* request,
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)> done) {
    for (auto& observer : observers_) {
      observer.OnGrpcMessage(*request);
    }

    ResponseType response;
    std::move(done).Run(grpc::Status::OK, response);
  }

  void OnRegisterEventHandlerDone(
      const ::grpc::Status& status,
      const ::assistant::api::RegisterEventHandlerResponse& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!status.ok()) {
      LOG(ERROR) << "Failed to register event handler. code: "
                 << status.error_code() << ". Msg: " << status.error_message();
    }
  }

  // AsyncServiceDriver implementation:
  void StartCQ(::grpc::ServerCompletionQueue* cq) override {
    rpc_event_driver_ =
        std::make_unique<RpcMethodDriver<RequestType, ResponseType>>(
            cq,
            base::BindRepeating(
                &THandlerInterface::AsyncService::RequestOnEventFromLibas,
                async_service_weak_factory_.GetWeakPtr()),
            base::BindRepeating(
                &EventHandlerDriver<THandlerInterface>::HandleEvent,
                weak_factory_.GetWeakPtr()));
  }

  std::unique_ptr<RpcMethodDriver<RequestType, ResponseType>> rpc_event_driver_;

  typename THandlerInterface::AsyncService service_;

  raw_ptr<GrpcLibassistantClient> libassistant_client_;
  const std::string assistant_service_address_;

  base::ObserverList<EventObserverType> observers_;

  // This sequence checker ensures that all callbacks are called on the main
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<typename THandlerInterface::AsyncService>
      async_service_weak_factory_{&service_};
  base::WeakPtrFactory<EventHandlerDriver<THandlerInterface>> weak_factory_{
      this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_EVENT_HANDLER_DRIVER_H_
