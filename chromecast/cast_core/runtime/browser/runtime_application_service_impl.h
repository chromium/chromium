// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace content {
class WebContents;
class WebUIControllerFactory;
}

namespace chromecast {

class CastContentWindow;
class MessagePortService;
class RuntimeApplicationBase;

class RuntimeApplicationServiceImpl : public RuntimeApplicationBase::Delegate {
 public:
  using StatusCallback = cast_receiver::RuntimeApplication::StatusCallback;

  RuntimeApplicationServiceImpl(
      std::unique_ptr<RuntimeApplicationBase> runtime_application,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      CastWebService& web_service);
  ~RuntimeApplicationServiceImpl() override;

  void Load(const cast::runtime::LoadApplicationRequest& request,
            StatusCallback callback);
  void Launch(const cast::runtime::LaunchApplicationRequest& request,
              StatusCallback callback);
  void Stop(const cast::runtime::StopApplicationRequest& request,
            StatusCallback callback);

  const std::string& app_id() { return runtime_application_->GetAppId(); }

  // RuntimeApplication::Delegate implementation:
  void NotifyApplicationStarted() override;
  void NotifyApplicationStopped(cast::common::StopReason::Type stop_reason,
                                int32_t net_error_code) override;
  void NotifyMediaPlaybackChanged(bool playing) override;
  void GetAllBindings(GetAllBindingsCallback callback) override;
  std::unique_ptr<MessagePortService> CreateMessagePortService() override;
  std::unique_ptr<content::WebUIControllerFactory> CreateWebUIControllerFactory(
      std::vector<std::string> hosts) override;
  content::WebContents* GetWebContents() override;
  cast_receiver::ContentWindowControls* GetContentWindowControls() override;

 private:
  // Creates the root CastWebView for this Cast session.
  CastWebView::Scoped CreateCastWebView();

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

  void OnAllBindingsReceived(
      GetAllBindingsCallback callback,
      cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or);

  std::unique_ptr<RuntimeApplicationBase> const runtime_application_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::raw_ref<CastWebService> web_service_;

  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

  // Controls for window, as a wrapper around a CastContentWindow instance.
  // NOTE: Must be declared after |cast_web_view_|.
  std::unique_ptr<cast_receiver::ContentWindowControls>
      content_window_controls_;

  absl::optional<cast::utils::GrpcServer> grpc_server_;
  absl::optional<cast::v2::CoreApplicationServiceStub> core_app_stub_;
  absl::optional<cast::v2::CoreMessagePortApplicationServiceStub>
      core_message_port_app_stub_;
  absl::optional<std::string> cast_media_service_grpc_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationServiceImpl> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_
