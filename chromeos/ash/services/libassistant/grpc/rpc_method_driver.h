// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_RPC_METHOD_DRIVER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_RPC_METHOD_DRIVER_H_

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/server_context.h"

namespace ash::libassistant {

// Implements async RPC driver for an RPC method.
// Request and Response are the RPC method's request and response protos.
//
// Sample usage for handling SetCapabilities() RPC method from service
// ConversationInterface:
//
//  set_capabilities_rpc_driver_.reset(
//      std::make_unique<RpcMethodDriver<SetCapabilitiesRequest,
//      SetCapabilitiesResponse>>(
//          cq,
//          base::BindRepeating(
//              &ConversationInterface::AsyncService::RequestSetCapabilities,
//              service_.WeakPtr()),
//          base::BindRepeating(
//              &ConversationInterfaceImpl::SetCapabilities,
//              conversation_interface_impl_.WeakPtr())));
//
template <class Request, class Response>
class RpcMethodDriver {
 public:
  // Callback object encapsulating service.Request##RpcMethod() which looks
  // for next incoming RPC.
  using ServiceRpcCallFn =
      base::RepeatingCallback<void(grpc::ServerContext*,
                                   Request*,
                                   grpc::ServerAsyncResponseWriter<Response>*,
                                   grpc::CompletionQueue*,
                                   grpc::ServerCompletionQueue*,
                                   void*)>;

  // Callback object for calling RPC async business logic implementation.
  using RpcImplAsyncFn = base::RepeatingCallback<void(
      grpc::ServerContext*,
      const Request*,
      base::OnceCallback<void(const grpc::Status&, const Response&)>)>;

  // Constructs the class and initializes the completion queue.
  // cq: CompletionQueue
  // service_rpc_call_fn: Callback object encapsulating
  //         service.Request##RpcMethod() which looks for next incoming RPC.
  // rpc_impl_async_fn: Callback object for calling implementation of
  //              business logic of the RPC.
  RpcMethodDriver(grpc::ServerCompletionQueue* cq,
                  ServiceRpcCallFn service_rpc_call_fn,
                  RpcImplAsyncFn rpc_impl_async_fn)
      : cq_(cq),
        service_rpc_call_fn_(service_rpc_call_fn),
        rpc_impl_async_fn_(rpc_impl_async_fn) {
    DCHECK(cq);
    RequestNextRpc();
  }

  ~RpcMethodDriver() = default;
  RpcMethodDriver(const RpcMethodDriver&) = delete;
  RpcMethodDriver& operator=(const RpcMethodDriver&) = delete;

 private:
  // Look for the next incoming RPC.
  void RequestNextRpc() {
    // Owned by CleanupAfterRpc() run at the end of the lifecycle of current
    // RPC.
    auto ctx = std::make_unique<grpc::ServerContext>();
    auto request = std::make_unique<Request>();
    auto responder =
        std::make_unique<grpc::ServerAsyncResponseWriter<Response>>(ctx.get());

    // Prestore valid pointers before std::move() nulls the smart pointers.
    auto* ctx_ptr = ctx.get();
    auto* request_ptr = request.get();
    auto* responder_ptr = responder.get();

    // A raw pointer has to be used here since |service_rpc_call_fn_| is
    // expecting void* as the parameter. It will be deleted by server cq
    // after being executed.
    auto* process_rpc_cb = new base::OnceCallback<void(bool)>(
        base::BindOnce(&RpcMethodDriver<Request, Response>::ProcessRpc,
                       weak_factory_.GetWeakPtr(), std::move(ctx),
                       std::move(request), std::move(responder)));

    DCHECK(service_rpc_call_fn_);
    service_rpc_call_fn_.Run(ctx_ptr, request_ptr, responder_ptr, cq_.get(),
                             cq_.get(), process_rpc_cb);
  }

  // Process the RPC received.
  void ProcessRpc(
      std::unique_ptr<grpc::ServerContext> ctx,
      std::unique_ptr<Request> request,
      std::unique_ptr<grpc::ServerAsyncResponseWriter<Response>> responder,
      bool ok) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!ok) {
      // If not okay, logs error and returns. Used data, i.e. ctx, will be
      // cleaned up automatically when unique_ptrs go out of scope.
      LOG(ERROR) << "OnEventFromLibas request not ok.";
      return;
    }

    // Start waiting for the next RPC.
    RequestNextRpc();

    ExecuteRpc(std::move(ctx), std::move(request), std::move(responder));
  }

  // Execute the RPC received.
  void ExecuteRpc(
      std::unique_ptr<grpc::ServerContext> ctx,
      std::unique_ptr<Request> request,
      std::unique_ptr<grpc::ServerAsyncResponseWriter<Response>> responder) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Prestore valid pointers before std::move() nulls the smart pointers.
    auto* ctx_ptr = ctx.get();
    auto* request_ptr = request.get();
    auto* responder_ptr = responder.get();

    auto* finish_rpc_cb = new base::OnceCallback<void(bool)>(
        base::BindOnce(&RpcMethodDriver<Request, Response>::CleanupAfterRpc,
                       weak_factory_.GetWeakPtr(), std::move(ctx),
                       std::move(request), std::move(responder)));

    auto async_cb = base::BindOnce(
        [](grpc::ServerAsyncResponseWriter<Response>* responder,
           base::OnceCallback<void(bool)>* finish_rpc_cb,
           const grpc::Status& status, const Response& response) {
          responder->Finish(response, status, finish_rpc_cb);
        },
        responder_ptr, finish_rpc_cb);

    DCHECK(rpc_impl_async_fn_);
    // Call the async implementation of the RPC business logic.
    rpc_impl_async_fn_.Run(ctx_ptr, request_ptr, std::move(async_cb));
  }

  void CleanupAfterRpc(
      std::unique_ptr<grpc::ServerContext> ctx,
      std::unique_ptr<Request> request,
      std::unique_ptr<grpc::ServerAsyncResponseWriter<Response>> responder,
      bool ignored_ok) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DVLOG(3) << "OnEventFromLibas is finished.";

    // Unique_ptrs will delete the objects after they go out of scope
    // so no manual data clean-up needed here.
  }

  // Owned by |ServicesInitializerBase|.
  raw_ptr<grpc::ServerCompletionQueue> cq_ = nullptr;

  ServiceRpcCallFn service_rpc_call_fn_;
  RpcImplAsyncFn rpc_impl_async_fn_;

  // This sequence checker ensures that all callbacks are called on the
  // main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RpcMethodDriver> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_RPC_METHOD_DRIVER_H_
