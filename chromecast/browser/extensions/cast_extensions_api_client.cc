// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extensions_api_client.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chromecast/browser/extensions/api/automation_internal/chromecast_automation_internal_api_delegate.h"
#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

namespace extensions {
namespace {
class CastWebViewGuestDelegate : public WebViewGuestDelegate {
 public:
  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    return true;
  }

  void OnShowContextMenu(int request_id) override {}
};
}  // namespace

CastExtensionsAPIClient::CastExtensionsAPIClient() {}

CastExtensionsAPIClient::~CastExtensionsAPIClient() {}

void CastExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  CastExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

WebViewGuestDelegate* CastExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return new CastWebViewGuestDelegate();
}

MessagingDelegate* CastExtensionsAPIClient::GetMessagingDelegate() {
  // The default implementation does nothing, which is fine.
  if (!messaging_delegate_)
    messaging_delegate_ = std::make_unique<MessagingDelegate>();
  return messaging_delegate_.get();
}

AutomationInternalApiDelegate*
CastExtensionsAPIClient::GetAutomationInternalApiDelegate() {
  if (!extensions_automation_api_delegate_) {
    extensions_automation_api_delegate_ =
        std::make_unique<ChromecastAutomationInternalApiDelegate>();
  }
  return extensions_automation_api_delegate_.get();
}

}  // namespace extensions
