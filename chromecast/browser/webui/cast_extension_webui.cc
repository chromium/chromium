// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_extension_webui.h"

#include <string>

#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/browser/webui/mojom/webui.mojom.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

namespace chromecast {

// static
std::unique_ptr<CastWebUI> CastWebUI::Create(content::WebUI* webui,
                                             const std::string& host,
                                             mojom::WebUiClient* client) {
  return std::make_unique<CastExtensionWebUI>(webui, host, client);
}

CastExtensionWebUI::CastExtensionWebUI(content::WebUI* webui,
                                       const std::string& host,
                                       mojom::WebUiClient* client)
    : CastWebUI(webui, host, client) {
  if (host == kCastWebUIHomeHost) {
    // We need an ExtensionWebContentsObserver to support the <webview> tag,
    // so make sure one exists (this is a no-op if one already does).
    extensions::CastExtensionWebContentsObserver::CreateForWebContents(
        web_contents_);
    extensions::ExtensionWebContentsObserver::GetForWebContents(web_contents_)
        ->dispatcher()
        ->set_delegate(this);
    if (!guest_view::GuestViewManager::FromBrowserContext(browser_context_)) {
      guest_view::GuestViewManager::CreateWithDelegate(
          browser_context_,
          std::make_unique<extensions::ExtensionsGuestViewManagerDelegate>(
              browser_context_));
    }
  }
}

CastExtensionWebUI::~CastExtensionWebUI() {}

content::WebContents* CastExtensionWebUI::GetAssociatedWebContents() const {
  return web_contents_;
}

}  // namespace chromecast
