// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

namespace chromecast {

class BindingsManagerWebRuntime;
class CastWebService;

class WebRuntimeApplication final : public RuntimeApplicationBase,
                                    public CastWebContents::Observer {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  WebRuntimeApplication(std::string cast_session_id,
                        cast::common::ApplicationConfig app_config,
                        CastWebService* web_service,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRuntimeApplication() override;

 private:
  // RuntimeApplication implementation:
  const GURL& GetApplicationUrl() const override;

  // RuntimeApplicationBase implementation:
  cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> HandlePortMessage(
      cast::web::Message message) override;
  void InitializeApplication(
      base::OnceClosure app_initialized_callback) override;
  bool IsStreamingApplication() const override;

  // CastWebContents::Observer implementation:
  void InnerContentsCreated(CastWebContents* inner_contents,
                            CastWebContents* outer_contents) override;

  void OnAllBindingsReceived(
      base::OnceClosure app_initialized_callback,
      cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or);
  void OnApplicationStateChanged(grpc::Status status);

  const GURL app_url_;
  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WebRuntimeApplication> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
