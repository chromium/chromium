// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/async_service_driver.h"
#include "chromeos/ash/services/libassistant/grpc/rpc_method_driver.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/action_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/action_service.grpc.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/query_interface.pb.h"

namespace assistant {
namespace api {
class GetActionServiceContextRequest;
class GetActionServiceContextResponse;
class HandleActionRequest;
class HandleActionResponse;
}  // namespace api
}  // namespace assistant

namespace ash::libassistant {

class GrpcLibassistantClient;

class ActionService : public AsyncServiceDriver {
 public:
  ActionService(::grpc::ServerBuilder* server_builder,
                GrpcLibassistantClient* libassistant_client,
                const std::string& assistant_service_address);
  ~ActionService() override;

  // ActionService does no own the ActionModules, so the ActionModules have to
  // be live as long as the ActionService.
  void RegisterActionModule(assistant_client::ActionModule* action_module);

  // This should be called only when we know the server is ready.
  void StartRegistration();

 private:
  // Handles the response of RegisterActionModuleRequest from libassistant.
  void OnRegistrationDone(
      const ::grpc::Status& status,
      const ::assistant::api::RegisterActionModuleResponse& response);

  // Handles HandleActionRequest from libassistant to
  // prepare/execute/interrupt one action. see action_interface.proto.
  void OnHandleActionRequest(
      grpc::ServerContext* context,
      const ::assistant::api::HandleActionRequest* request,
      base::OnceCallback<void(const grpc::Status&,
                              const ::assistant::api::HandleActionResponse&)>
          done);

  // If the requested operation is
  // * PREPARE, returns a new action.
  // * EXECUTE, returns the prepared action if there is one created before
  // because of PREPARE request, or create a new one.
  // * INTERRUPT, returns the alive action from `alive_actions_` if there is one
  // existing.
  // Returned actions are owned by `alive_actions_` and will be deleted when
  // `OnActionDone()`.
  assistant_client::ActionModule::Action* PrepareAction(
      const ::assistant::api::HandleActionRequest* request);

  // Sends the action result back to libassistant and clean up actions.
  void OnActionDone(
      base::OnceCallback<void(const grpc::Status&,
                              const ::assistant::api::HandleActionResponse&)>
          done,
      const std::string& action_id,
      const assistant_client::ActionModule::Result& result);

  // Handles GetActionServiceContextRequest from libassistant.
  void OnGetActionServiceContextRequest(
      grpc::ServerContext* context,
      const ::assistant::api::GetActionServiceContextRequest* request,
      base::OnceCallback<
          void(const grpc::Status&,
               const ::assistant::api::GetActionServiceContextResponse&)> done);

  // AsyncServiceDriver:
  void StartCQ(::grpc::ServerCompletionQueue* cq) override;

  raw_ptr<assistant_client::ActionModule> action_module_ = nullptr;

  // Map with the concatenated |convesation_id| and |interaction_id| from
  // |HandleActionRequest| as the key. The value is a pair of the action name
  // and the action itself.
  base::flat_map<
      std::string,
      std::pair<std::string,
                std::unique_ptr<assistant_client::ActionModule::Action>>>
      alive_actions_;

  // Owned by `GrpcServicesInitializer`.
  raw_ptr<GrpcLibassistantClient> libassistant_client_ = nullptr;

  const std::string assistant_service_address_;

  ::assistant::api::ActionService::AsyncService service_;

  std::unique_ptr<RpcMethodDriver<::assistant::api::HandleActionRequest,
                                  ::assistant::api::HandleActionResponse>>
      action_handler_driver_;
  std::unique_ptr<
      RpcMethodDriver<::assistant::api::GetActionServiceContextRequest,
                      ::assistant::api::GetActionServiceContextResponse>>
      service_context_driver_;

  // This sequence checker ensures that all callbacks are called on the main
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<::assistant::api::ActionService::AsyncService>
      async_service_weak_factory_{&service_};
  base::WeakPtrFactory<ActionService> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_SERVICE_H_
