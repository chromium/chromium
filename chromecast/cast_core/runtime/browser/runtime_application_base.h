// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_service_grpc_impl.h"
#include "chromecast/cast_core/runtime/browser/runtime_message_port_application_service_grpc_impl.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.grpc.pb.h"

namespace chromecast {

class CastWebService;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase
    : public RuntimeApplication,
      public GrpcServer,
      public RuntimeApplicationServiceDelegate,
      public RuntimeMessagePortApplicationServiceDelegate {
 public:
  ~RuntimeApplicationBase() override;

 protected:
  using CoreApplicationServiceGrpc = cast::v2::CoreApplicationService::Stub;

  // |web_service| is expected to exist for the lifetime of this instance.
  RuntimeApplicationBase(mojom::RendererType renderer_type_used,
                         CastWebService* web_service,
                         scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication();

  // Sets that the application has been started - the meaning of which is
  // application-specific.
  void SetApplicationStarted();

  // Returns current TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // Returns a pointer to CastWebService.
  CastWebService* cast_web_service() const { return web_service_; }

  // Returns a pointer to a CastWebView.
  CastWebView* cast_web_view() { return cast_web_view_.get(); }

  // RuntimeApplication implementation:
  url_rewrite::UrlRequestRewriteRulesManager* GetUrlRewriteRulesManager()
      override;

  // RuntimeApplicationServiceDelegate implementation:
  void SetUrlRewriteRules(const cast::v2::SetUrlRewriteRulesRequest& request,
                          cast::v2::SetUrlRewriteRulesResponse* response,
                          GrpcMethod* callback) override;

  // Processes an incoming |message|, returning the status of this processing in
  // |response| after being received over gRPC.
  virtual void HandleMessage(const cast::web::Message& message,
                             cast::web::MessagePortStatus* response) = 0;

  // Called following the creation of a CastWebView, with which
  // |cast_web_contents  is associated. Must set the application URL as a
  // result.
  virtual void InitializeApplication(CoreApplicationServiceGrpc* grpc_stub,
                                     CastWebContents* cast_web_contents) = 0;

  std::unique_ptr<CoreApplicationServiceGrpc> core_app_stub_;

 private:
  // RuntimeApplication implementation:
  bool Load(const cast::runtime::LoadApplicationRequest& request) final;
  bool Launch(const cast::runtime::LaunchApplicationRequest& request) final;

  // Called when a new CastWebView is created.
  void CreateCastWebView();

  // Called following Launch() on |task_runner_|.
  void FinishLaunch(std::string core_application_service_endpoint);

  // RuntimeMessagePortApplicationServiceDelegate implementation:
  void PostMessage(const cast::web::Message& request,
                   cast::web::MessagePortStatus* response,
                   GrpcMethod* callback) override;

  // gRPC RPC Wrappers.
  cast::v2::RuntimeApplicationService::AsyncService grpc_app_service_;
  cast::v2::RuntimeMessagePortApplicationService::AsyncService
      grpc_message_port_service_;

  // The |web_service_| used to create |cast_web_view_|.
  CastWebService* const web_service_;
  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

  std::unique_ptr<url_rewrite::UrlRequestRewriteRulesManager>
      url_rewrite_rules_manager_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Set to true when StopApplication() is called. This variable is required
  // rather than always executing StopApplication() in the dtor due to how
  // virtual function calls are handled during destruction.
  bool is_application_stopped_ = false;

  // Renderer type used by this application.
  mojom::RendererType renderer_type_;

  base::WeakPtrFactory<RuntimeApplicationBase> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
