// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/action_service.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/callback_utils.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/action_args.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/internal_constants.h"

namespace ash::libassistant {

namespace {

std::string GetActionId(const ::assistant::api::HandleActionRequest* request) {
  return request->conversation_id() + ":" +
         base::NumberToString(request->interaction_id());
}

}  // namespace

ActionService::ActionService(::grpc::ServerBuilder* server_builder,
                             GrpcLibassistantClient* libassistant_client,
                             const std::string& assistant_service_address)
    : AsyncServiceDriver(server_builder),
      libassistant_client_(libassistant_client),
      assistant_service_address_(assistant_service_address),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(server_builder);
  DCHECK(libassistant_client_);

  server_builder_->RegisterService(&service_);
}

ActionService::~ActionService() = default;

void ActionService::RegisterActionModule(
    assistant_client::ActionModule* action_module) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!action_module_);

  action_module_ = action_module;
  StartRegistration();
}

void ActionService::StartRegistration() {
  ::assistant::api::RegisterActionModuleRequest request;
  for (const auto& action : action_module_->GetSupportedActions()) {
    chromeos::libassistant::PopulateRegisterActionModuleRequest(action,
                                                                &request);
  }

  auto* action_handler = request.mutable_action_handler();
  action_handler->set_server_address(assistant_service_address_);
  action_handler->set_service_name(chromeos::assistant::kActionServiceName);
  action_handler->set_handler_method(
      chromeos::assistant::kHandleActionMethodName);

  auto* context_provider = request.mutable_context_provider();
  context_provider->set_server_address(assistant_service_address_);
  context_provider->set_service_name(chromeos::assistant::kActionServiceName);
  context_provider->set_handler_method(
      chromeos::assistant::kGetContextMethodName);

  constexpr int kMaxRegisterRetry = 3;
  constexpr int kRegisterTimeoutInMs = 2000;
  StateConfig config;
  config.max_retries = kMaxRegisterRetry;
  config.timeout_in_ms = kRegisterTimeoutInMs;

  libassistant_client_->CallServiceMethod(
      request,
      base::BindOnce(&ActionService::OnRegistrationDone,
                     weak_factory_.GetWeakPtr()),
      std::move(config));
}

void ActionService::OnRegistrationDone(
    const ::grpc::Status& status,
    const ::assistant::api::RegisterActionModuleResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    LOG(ERROR) << "Registration failed with status: " << status.error_code()
               << " and message: " << status.error_message();
    return;
  }

  bool has_failure = false;
  for (const auto& result : response.register_result()) {
    DVLOG(3) << "Client op <" << result.first
             << "> registration status = " << result.second;
    if (result.second !=
        ::assistant::api::RegisterActionModuleResponse_Status_SUCCESS) {
      has_failure = true;
    }
  }

  if (has_failure) {
    LOG(ERROR) << "Registration failed.";
  }
}

void ActionService::OnHandleActionRequest(
    grpc::ServerContext* context,
    const ::assistant::api::HandleActionRequest* request,
    base::OnceCallback<void(const grpc::Status&,
                            const ::assistant::api::HandleActionResponse&)>
        done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);

  if (!request->has_conversation_id() || !request->has_interaction_id()) {
    LOG(ERROR) << "Received invalid HandleActionRequest.";
    ::assistant::api::HandleActionResponse response;
    std::move(done).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                     "HandleActionRequest missing arguments."),
                        response);
    return;
  }

  assistant_client::ActionModule::Action* action = PrepareAction(request);
  if (!action) {
    // TODO: Set the proper operation status in response.
    // If the status is not OK, it will ignore the response and generate
    // `Result::Error` and send to server.
    LOG(ERROR) << "PrepareAction returns nullptr.";
    ::assistant::api::HandleActionResponse response;
    std::move(done).Run(grpc::Status::OK, response);
    return;
  }

  DVLOG(3) << "Received request: operation=" << request->operation()
           << ", client_op_name=" << request->client_op_name();
  switch (request->operation()) {
    case ::assistant::api::HandleActionRequest_Operation_PREPARE: {
      ::assistant::api::HandleActionResponse response;
      std::move(done).Run(grpc::Status::OK, response);
      return;
    }
    case ::assistant::api::HandleActionRequest_Operation_EXECUTE: {
      const std::string& action_id = GetActionId(request);
      action->Execute(ToStdFunction(base::BindOnce(
          &ActionService::OnActionDone, weak_factory_.GetWeakPtr(),
          std::move(done), action_id)));
      return;
    }
    case ::assistant::api::HandleActionRequest_Operation_INTERRUPT: {
      // TODO: Add interrupt logic.
      const std::string& action_id = GetActionId(request);
      DVLOG(2) << "Action is interrupted, id: " << action_id;
      return;
    }
    case ::assistant::api::HandleActionRequest_Operation_TERMINATE: {
      const std::string& action_id = GetActionId(request);
      const auto action_iter = alive_actions_.find(action_id);
      if (action_iter != alive_actions_.end()) {
        DVLOG(3) << "Destroyed action without execution, id: " << action_id
                 << ", name: " << action_iter->second.first;
        alive_actions_.erase(action_id);
      } else {
        LOG(ERROR)
            << "The action with id: " << action_id
            << " doesn't exist in |alive_actions_|. This should never happen.";
      }
      return;
    }
  }
}

assistant_client::ActionModule::Action* ActionService::PrepareAction(
    const ::assistant::api::HandleActionRequest* request) {
  const std::string& action_id = GetActionId(request);
  const auto action_iter = alive_actions_.find(action_id);
  // Try to retrieve the action from the alive actions. This is for retrieving
  // the action that has prepare phase for execute operation or the action is
  // executing for interrupt operation.
  if (action_iter != alive_actions_.end()) {
    return action_iter->second.second.get();
  }

  // Never try to create a new action if the operation is interrupting or
  // terminating.
  if (request->operation() ==
          ::assistant::api::HandleActionRequest_Operation_INTERRUPT ||
      request->operation() ==
          ::assistant::api::HandleActionRequest_Operation_TERMINATE) {
    return nullptr;
  }

  if (!request->has_client_op_name()) {
    LOG(ERROR) << "Failed to create the action because of no client op name in "
                  "the request.";
    return nullptr;
  }
  const std::string& action_name = request->client_op_name();

  // ActionModule returns the raw pointer of a new action and transfers
  // the ownership. The raw pointer is used to cross the ABI boundaries.
  auto* action =
      action_module_->CreateAction(action_name, ActionArgs(*request));
  if (!action) {
    LOG(ERROR) << "Action module failed to create action : " << action_name;
    return nullptr;
  }

  alive_actions_.insert(
      {action_id, std::make_pair(action_name, base::WrapUnique(action))});
  return action;
}

void ActionService::OnActionDone(
    base::OnceCallback<void(const grpc::Status&,
                            const ::assistant::api::HandleActionResponse&)>
        done,
    const std::string& action_id,
    const assistant_client::ActionModule::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ::assistant::api::HandleActionResponse response;
  chromeos::libassistant::PopulateHandleActionResponse(result, &response);

  const auto action_iter = alive_actions_.find(action_id);
  if (action_iter != alive_actions_.end()) {
    DVLOG(3) << "Finished executing action with id: " << action_id
             << " and name: " << action_iter->second.first;
    // Delete the action in the future to prevent deadlock on the WaitAction.
    // When the WaitAction runs its callback in `OnScheduledWaitDone()` with
    // lock, the callback (this function) will delete the action here. In the
    // dtor of the WaitAction, it will try to call `OnScheduledWaitDone()` to
    // clean up, which will end up with deadlock.
    auto action = std::move(action_iter->second.second);
    alive_actions_.erase(action_id);
    task_runner_->DeleteSoon(FROM_HERE, action.release());
  } else {
    LOG(ERROR)
        << "The action with id: " << action_id
        << " doesn't exist in |alive_actions_|. This should never happen.";
  }

  std::move(done).Run(grpc::Status::OK, response);
}

void ActionService::OnGetActionServiceContextRequest(
    grpc::ServerContext* context,
    const ::assistant::api::GetActionServiceContextRequest* request,
    base::OnceCallback<
        void(const grpc::Status&,
             const ::assistant::api::GetActionServiceContextResponse&)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(3) << "Getting service context.";
  ::assistant::api::GetActionServiceContextResponse response;
  chromeos::libassistant::PopulateGetActionServiceContextResponse(
      *action_module_, &response);
  std::move(done).Run(grpc::Status::OK, response);
}

void ActionService::StartCQ(::grpc::ServerCompletionQueue* cq) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  action_handler_driver_ = std::make_unique<
      RpcMethodDriver<::assistant::api::HandleActionRequest,
                      ::assistant::api::HandleActionResponse>>(
      cq,
      base::BindRepeating(
          &::assistant::api::ActionService::AsyncService::RequestHandleAction,
          async_service_weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ActionService::OnHandleActionRequest,
                          weak_factory_.GetWeakPtr()));

  service_context_driver_ = std::make_unique<
      RpcMethodDriver<::assistant::api::GetActionServiceContextRequest,
                      ::assistant::api::GetActionServiceContextResponse>>(
      cq,
      base::BindRepeating(&::assistant::api::ActionService::AsyncService::
                              RequestGetActionServiceContext,
                          async_service_weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ActionService::OnGetActionServiceContextRequest,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace ash::libassistant
