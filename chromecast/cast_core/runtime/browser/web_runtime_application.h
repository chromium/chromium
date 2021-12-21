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
  WebRuntimeApplication(CastWebService* web_service,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRuntimeApplication() override;

 private:
  // RuntimeApplicationBase implementation:
  void HandleMessage(const cast::web::Message& message,
                     cast::web::MessagePortStatus* response) override;
  void InitializeApplication(CoreApplicationServiceGrpc* grpc_stub,
                             CastWebContents* cast_web_contents) override;

  // CastWebContents::Observer implementation:
  void InnerContentsCreated(CastWebContents* inner_contents,
                            CastWebContents* outer_contents) override;

  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
