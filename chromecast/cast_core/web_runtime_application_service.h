// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_WEB_RUNTIME_APPLICATION_SERVICE_H_
#define CHROMECAST_CAST_CORE_WEB_RUNTIME_APPLICATION_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc_server.h"
#include "chromecast/cast_core/runtime_application_service.h"
#include "chromecast/cast_core/runtime_application_service_grpc_impl.h"
#include "chromecast/cast_core/runtime_message_port_application_service_grpc_impl.h"
#include "third_party/openscreen/src/cast/cast_core/api/v2/core_application_service.grpc.pb.h"

namespace chromecast {

class BindingsManagerWebRuntime;
class CastWebService;
class UrlRewriteRulesAdapter;

class WebRuntimeApplicationService final
    : public RuntimeApplicationService,
      public GrpcServer,
      public RuntimeApplicationServiceDelegate,
      public RuntimeMessagePortApplicationServiceDelegate,
      public CastWebView::Delegate,
      public CastWebContents::Observer {
 public:
  WebRuntimeApplicationService(
      CastWebService* web_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRuntimeApplicationService() override;

  // RuntimeApplicationService implementation:
  bool Load(const cast::runtime::LoadApplicationRequest& request) override;
  bool Launch(const cast::runtime::LaunchApplicationRequest& request) override;

  // RuntimeApplicationServiceDelegate implementation:
  void SetUrlRewriteRules(const cast::v2::SetUrlRewriteRulesRequest& request,
                          cast::v2::SetUrlRewriteRulesResponse* response,
                          GrpcMethod* callback) override;

  // RuntimeMessagePortApplicationServiceDelegate implementation:
  void PostMessage(const cast::web::Message& request,
                   cast::web::MessagePortStatus* response,
                   GrpcMethod* callback) override;

  // CastContentWindow::Delegate implementation:
  void OnWindowDestroyed() override;
  bool CanHandleGesture(GestureType gesture_type) override;
  void ConsumeGesture(GestureType gesture_type,
                      GestureHandledCallback handled_callback) override;
  void OnVisibilityChange(VisibilityType visibility_type) override;

  // CastWebContents::Observer implementation:
  void RenderFrameCreated(
      int render_process_id,
      int render_frame_id,
      service_manager::InterfaceProvider* frame_interfaces,
      blink::AssociatedInterfaceProvider* frame_associated_interfaces) override;

 private:
  void FinishLaunch(const std::string& core_application_service_address);

  CastWebService* const web_service_;

  cast::v2::RuntimeApplicationService::AsyncService grpc_app_service_;
  cast::v2::RuntimeMessagePortApplicationService::AsyncService
      grpc_message_port_service_;
  std::unique_ptr<cast::v2::CoreApplicationService::Stub> core_app_stub_;

  std::string app_url_;
  std::string display_name_;
  CastWebView::Scoped cast_web_view_;

  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;
  std::unique_ptr<UrlRewriteRulesAdapter> url_rewrite_adapter_;

  base::WeakPtrFactory<WebRuntimeApplicationService> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_WEB_RUNTIME_APPLICATION_SERVICE_H_
