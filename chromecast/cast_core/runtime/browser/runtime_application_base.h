// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_message_port_application_service.castcore.pb.h"

namespace chromecast {

class CastWebService;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase : public RuntimeApplication {
 public:
  ~RuntimeApplicationBase() override;

 protected:
  using CoreApplicationServiceGrpc = cast::v2::CoreApplicationService::Stub;

  // |web_service| is expected to exist for the lifetime of this instance.
  RuntimeApplicationBase(std::string cast_session_id,
                         cast::common::ApplicationConfig app_config,
                         mojom::RendererType renderer_type_used,
                         CastWebService* web_service,
                         scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication();

  // Reports the application |state| to Cast Core.
  void SetApplicationState(cast::v2::ApplicationStatusRequest::State state,
                           StatusCallback callback);

  // Returns current TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // Returns a stub to CoreApplicationService.
  cast::v2::CoreApplicationServiceStub* core_app_stub() {
    return &*core_app_stub_;
  }

  // Returns a stub to CoreMessagePortApplicationService.
  cast::v2::CoreMessagePortApplicationServiceStub*
  core_message_port_app_stub() {
    return &*core_message_port_app_stub_;
  }

  // RuntimeApplication implementation:
  CastWebContents* GetCastWebContents() override;
  const std::string& GetCastMediaServiceEndpoint() const override;

  // Initializes the Cast application. If initialization passes, the
  // |app_initialized_callback| is called.
  virtual void InitializeApplication(
      base::OnceClosure app_initialized_callback) = 0;

  // Processes an incoming |message|, returning the status of this processing in
  // |response| after being received over gRPC.
  virtual cast::utils::GrpcStatusOr<cast::web::MessagePortStatus>
  HandlePortMessage(cast::web::Message message) = 0;

  // RuntimeApplication implementation:
  const cast::common::ApplicationConfig& GetAppConfig() const override;
  const std::string& GetCastSessionId() const override;
  void Load(cast::runtime::LoadApplicationRequest request,
            StatusCallback callback) final;
  void Launch(cast::runtime::LaunchApplicationRequest request,
              StatusCallback callback) final;

 private:
  // RuntimeApplicationService handlers:
  void HandleSetUrlRewriteRules(
      cast::v2::SetUrlRewriteRulesRequest request,
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
          reactor);

  // RuntimeMessagePortApplicationService handlers:
  void HandlePostMessage(cast::web::Message request,
                         cast::v2::RuntimeMessagePortApplicationServiceHandler::
                             PostMessage::Reactor* reactor);

  // Creates the root CastWebView for this Cast session.
  void CreateCastWebView();

  // Notifies that the application has been initialized.
  void OnApplicationInitialized();

  const std::string cast_session_id_;
  const cast::common::ApplicationConfig app_config_;

  // The |web_service_| used to create |cast_web_view_|.
  CastWebService* const web_service_;
  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  absl::optional<cast::utils::GrpcServer> grpc_server_;
  absl::optional<cast::v2::CoreApplicationServiceStub> core_app_stub_;
  absl::optional<cast::v2::CoreMessagePortApplicationServiceStub>
      core_message_port_app_stub_;
  absl::optional<std::string> cast_media_service_grpc_endpoint_;

  // Flags if application is stopped.
  bool is_application_running_ = false;

  // Renderer type used by this application.
  mojom::RendererType renderer_type_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationBase> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
