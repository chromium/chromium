// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "base/containers/contains.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/cast_core/runtime/browser/grpc_resource_data_source.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "url/gurl.h"

namespace chromecast {

GrpcWebUiControllerFactory::GrpcWebUiControllerFactory(
    const std::vector<std::string> hosts,
    cast::v2::CoreApplicationServiceStub* core_app_service_stub)
    : hosts_(std::move(hosts)), core_app_service_stub_(core_app_service_stub) {}

GrpcWebUiControllerFactory::~GrpcWebUiControllerFactory() = default;

content::WebUI::TypeID GrpcWebUiControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (base::Contains(hosts_, url.host())) {
    return const_cast<GrpcWebUiControllerFactory*>(this);
  }
  return content::WebUI::kNoWebUI;
}

bool GrpcWebUiControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
}

std::unique_ptr<content::WebUIController>
GrpcWebUiControllerFactory::CreateWebUIControllerForURL(content::WebUI* web_ui,
                                                        const GURL& url) {
  static std::once_flag flag;
  std::call_once(flag, [this, web_ui] {
    auto cast_resources = std::make_unique<GrpcResourceDataSource>(
        kCastWebUIResourceHost, false /* for_webui */,
        this->core_app_service_stub_);
    content::URLDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                std::move(cast_resources));
  });

  return GrpcWebUIController::Create(web_ui, url.host(),
                                     core_app_service_stub_);
}

}  // namespace chromecast
