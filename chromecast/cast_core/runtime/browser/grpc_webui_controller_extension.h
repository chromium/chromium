// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_EXTENSION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_EXTENSION_H_

#include "chromecast/cast_core/runtime/browser/grpc_webui_controller.h"
#include "extensions/browser/extension_function_dispatcher.h"

namespace content {
class WebContents;
class WebUI;
}  // namespace content

namespace chromecast {

// This class is used for rendering web contents of a Cast application inside a
// |CastWebView|. For example, a Cast IdleScreen
class GrpcWebUIControllerExtension
    : public GrpcWebUIController,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  GrpcWebUIControllerExtension(
      content::WebUI* webui,
      const std::string host,
      cast::v2::CoreApplicationServiceStub* core_app_service_stub);
  ~GrpcWebUIControllerExtension() override;

  // extensions::ExtensionFunctionDispatcher::Delegate implementation:
  content::WebContents* GetAssociatedWebContents() const override;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_EXTENSION_H_
