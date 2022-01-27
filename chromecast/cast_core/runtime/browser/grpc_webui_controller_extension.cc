// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_extension.h"
#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "chromecast/browser/webui/constants.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

namespace chromecast {

// static
std::unique_ptr<GrpcWebUIController> GrpcWebUIController::Create(
    content::WebUI* webui,
    const std::string host,
    cast::v2::CoreApplicationServiceStub* core_app_service_stub) {
  return std::make_unique<GrpcWebUIControllerExtension>(webui, host,
                                                        core_app_service_stub);
}

GrpcWebUIControllerExtension::GrpcWebUIControllerExtension(
    content::WebUI* webui,
    const std::string host,
    cast::v2::CoreApplicationServiceStub* core_app_service_stub)
    : GrpcWebUIController(webui, host, core_app_service_stub) {
  if (host == kCastWebUIHomeHost) {
    // We need an ExtensionWebContentsObserver to support the <webview> tag,
    // so make sure one exists (this is a no-op if one already does).
    extensions::CastExtensionWebContentsObserver::CreateForWebContents(
        web_contents());
    extensions::ExtensionWebContentsObserver::GetForWebContents(web_contents())
        ->dispatcher()
        ->set_delegate(this);
    if (!guest_view::GuestViewManager::FromBrowserContext(browser_context())) {
      guest_view::GuestViewManager::CreateWithDelegate(
          browser_context(),
          std::make_unique<extensions::ExtensionsGuestViewManagerDelegate>(
              browser_context()));
    }
  }
}

GrpcWebUIControllerExtension::~GrpcWebUIControllerExtension() = default;

content::WebContents* GrpcWebUIControllerExtension::GetAssociatedWebContents()
    const {
  DCHECK(web_contents());
  return web_contents();
}

}  // namespace chromecast
