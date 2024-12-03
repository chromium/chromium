// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_web_view.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace glic {

GlicWebView::GlicWebView(content::BrowserContext* browser_context)
    : views::WebView(browser_context) {}
GlicWebView::~GlicWebView() = default;

void GlicWebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

BEGIN_METADATA(GlicWebView)
END_METADATA

}  // namespace glic
