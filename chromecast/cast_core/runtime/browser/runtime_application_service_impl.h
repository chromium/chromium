// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace cast_receiver {
class MessagePortService;
class StreamingConfigManager;
}  // namespace cast_receiver

namespace content {
class WebContents;
class WebUIControllerFactory;
}  // namespace content

namespace chromecast {

class CastContentWindow;
class MessagePortServiceGrpc;

class RuntimeApplicationServiceImpl : public cast_receiver::EmbedderApplication,
                                      public CastWebContents::Observer {
 public:
  using StatusCallback = cast_receiver::RuntimeApplication::StatusCallback;

  RuntimeApplicationServiceImpl(
      std::unique_ptr<cast_receiver::RuntimeApplication> runtime_application,
      cast::common::ApplicationConfig config,
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

  // EmbedderApplication implementation:
  void NotifyApplicationStarted() override;
  void NotifyApplicationStopped(ApplicationStopReason stop_reason,
                                int32_t net_error_code) override;
  void NotifyMediaPlaybackChanged(bool playing) override;
  void GetAllBindings(GetAllBindingsCallback callback) override;
  cast_receiver::MessagePortService* GetMessagePortService() override;
  std::unique_ptr<content::WebUIControllerFactory> CreateWebUIControllerFactory(
      std::vector<std::string> hosts) override;
  content::WebContents* GetWebContents() override;
  cast_receiver::ContentWindowControls* GetContentWindowControls() override;
#if !BUILDFLAG(IS_CAST_DESKTOP_BUILD)
  cast_receiver::StreamingConfigManager* GetStreamingConfigManager() override;
#endif
  void NavigateToPage(const GURL& url) override;

 private:
  // Gets the current |message_port_service_|, attempting to create it if it
  // does not yet exist.
  MessagePortServiceGrpc* GetMessagePortServiceGrpc();

  // Creates the root CastWebView for this Cast session.
  CastWebView::Scoped CreateCastWebView();

  // Helper functions for processing proto types.
  void SetTouchInput(cast::common::TouchInput::Type state);
  void SetVisibility(cast::common::Visibility::Type state);
  void SetMediaBlocking(cast::common::MediaState::Type state);

  // Called on an error is hit during running of cast mirroring or remoting.
  void OnStreamingApplicationError(cast_receiver::Status status);

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

  // Returns if app is audio only.
  bool IsAudioOnly() const;

  // Returns if current session is enabled for dev.
  bool IsEnabledForDev() const;

  // Returns if touch input is allowed.
  bool IsTouchInputAllowed() const;

  // Returns renderer features.
  base::Value::Dict GetRendererFeatures() const;

  // Returns whether feature permissions should be enforced.
  bool IsFeaturePermissionsEnforced() const;

  // CastWebContents::Observer overrides.
  void InnerContentsCreated(CastWebContents* inner_contents,
                            CastWebContents* outer_contents) override;

  const cast::common::ApplicationConfig config_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ref<CastWebService> web_service_;

  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

  // Controls for window, as a wrapper around a CastContentWindow instance.
  // NOTE: Must be declared after |cast_web_view_|.
  std::unique_ptr<cast_receiver::ContentWindowControls>
      content_window_controls_;

  // Manages access and retrieval of the StreamingConfig for a streaming session
  // initiated by the owning application.
  std::unique_ptr<cast_receiver::StreamingConfigManager>
      streaming_config_manager_;

  // Shared MessagePortService implementation for this application instance to
  // use.
  std::unique_ptr<MessagePortServiceGrpc> message_port_service_;

  std::optional<cast::utils::GrpcServer> grpc_server_;
  std::optional<cast::v2::CoreApplicationServiceStub> core_app_stub_;
  std::optional<cast::v2::CoreMessagePortApplicationServiceStub>
      core_message_port_app_stub_;
  std::optional<std::string> cast_media_service_grpc_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<cast_receiver::RuntimeApplication> const runtime_application_;

  base::WeakPtrFactory<RuntimeApplicationServiceImpl> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_IMPL_H_
