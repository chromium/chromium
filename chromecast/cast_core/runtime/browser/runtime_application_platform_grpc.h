// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_GRPC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ref.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace content {
class WebUIControllerFactory;
}

namespace chromecast {

class MessagePortService;

class RuntimeApplicationPlatformGrpc : public RuntimeApplicationPlatform {
 public:
  RuntimeApplicationPlatformGrpc(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::string cast_session_id,
      Client& client);
  ~RuntimeApplicationPlatformGrpc() override;

  // RuntimeApplicationPlatform implementation:
  void Load(cast::runtime::LoadApplicationRequest request,
            RuntimeApplication::StatusCallback callback) override;
  void Launch(cast::runtime::LaunchApplicationRequest request,
              RuntimeApplication::StatusCallback callback) override;
  void NotifyApplicationStarted() override;
  void NotifyApplicationStopped(cast::common::StopReason::Type stop_reason,
                                int32_t net_error_code) override;
  void NotifyMediaPlaybackChanged(bool playing) override;
  void GetAllBindingsAsync(GetAllBindingsCB callback) override;
  std::unique_ptr<MessagePortService> CreateMessagePortService() override;
  std::unique_ptr<content::WebUIControllerFactory> CreateWebUIControllerFactory(
      std::vector<std::string> hosts) override;

 private:
  // RuntimeApplicationService handlers:
  void HandleSetUrlRewriteRules(
      cast::v2::SetUrlRewriteRulesRequest request,
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
          reactor);
  void HandleSetMediaState(
      cast::v2::SetMediaStateRequest request,
      cast::v2::RuntimeApplicationServiceHandler::SetMediaState::Reactor*
          reactor);
  void HandleSetVisibility(
      cast::v2::SetVisibilityRequest request,
      cast::v2::RuntimeApplicationServiceHandler::SetVisibility::Reactor*
          reactor);
  void HandleSetTouchInput(
      cast::v2::SetTouchInputRequest request,
      cast::v2::RuntimeApplicationServiceHandler::SetTouchInput::Reactor*
          reactor);

  // RuntimeMessagePortApplicationService handlers:
  void HandlePostMessage(cast::web::Message request,
                         cast::v2::RuntimeMessagePortApplicationServiceHandler::
                             PostMessage::Reactor* reactor);

  // GetAllBindingsAsync() callback.
  void OnAllBindingsReceived(
      GetAllBindingsCB callback,
      cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or);

  base::raw_ref<Client> const client_;
  std::string session_id_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  absl::optional<cast::utils::GrpcServer> grpc_server_;
  absl::optional<cast::v2::CoreApplicationServiceStub> core_app_stub_;
  absl::optional<cast::v2::CoreMessagePortApplicationServiceStub>
      core_message_port_app_stub_;
  absl::optional<std::string> cast_media_service_grpc_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationPlatformGrpc> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_GRPC_H_
