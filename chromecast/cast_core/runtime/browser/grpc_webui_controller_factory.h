// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_FACTORY_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_FACTORY_H_

#include "content/public/browser/web_ui_controller_factory.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"

namespace content {
class BrowserContext;
class WebUI;
class WebUIController;
}  // namespace content

namespace chromecast {

// WebUIController Factory that uses gRPC for IPC. It determines which WebUIType
// to use based on the URL and creates an instance of GrpcWebUI for Urls of the
// form chrome://*.
class GrpcWebUiControllerFactory : public content::WebUIControllerFactory {
 public:
  GrpcWebUiControllerFactory(
      const std::vector<std::string> hosts,
      cast::v2::CoreApplicationServiceStub* core_app_service_stub);
  GrpcWebUiControllerFactory(const GrpcWebUiControllerFactory&) = delete;
  GrpcWebUiControllerFactory& operator=(const GrpcWebUiControllerFactory&) =
      delete;
  ~GrpcWebUiControllerFactory() override;

  // content::WebUIControllerFactory implementation:
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;

  // Gets WebUI type for the url.
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;

  // For CastCore, this function only creates an instance of GrpcWebUI
  // and does not create a WebUIController as it does not exist on core side.
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const std::vector<std::string> hosts_;
  cast::v2::CoreApplicationServiceStub* const core_app_service_stub_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_FACTORY_H_
