// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extension_page.h"

#include <utility>

#include "chromecast/browser/cast_content_window_aura.h"
#include "chromecast/browser/cast_extension_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"

namespace chromecast {

ExtensionPage::ExtensionPage(
    const CastWebContents::InitParams& init_params,
    const CastContentWindow::CreateParams& window_params,
    std::unique_ptr<CastExtensionHost> extension_host,
    CastWindowManager* window_manager)
    : window_(std::make_unique<CastContentWindowAura>(window_params,
                                                      window_manager)),
      extension_host_(std::move(extension_host)),
      cast_web_contents_(extension_host_->host_contents(), init_params) {
  content::WebContentsObserver::Observe(web_contents());
}

ExtensionPage::~ExtensionPage() {
  content::WebContentsObserver::Observe(nullptr);
}

content::WebContents* ExtensionPage::web_contents() const {
  return extension_host_->host_contents();
}

CastWebContents* ExtensionPage::cast_web_contents() {
  return &cast_web_contents_;
}

void ExtensionPage::Launch() {
  extension_host_->CreateRendererSoon();
}

void ExtensionPage::InitializeWindow() {
  window_->GrantScreenAccess();
  window_->CreateWindowForWebContents(
      &cast_web_contents_, mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);
}

void ExtensionPage::RenderFrameCreated(content::RenderFrameHost* frame_host) {
  // Set the initial top-level main frame to be transparent and focus it once
  // the renderer frame is made.
  if (frame_host == web_contents()->GetMainFrame()) {
    frame_host->GetView()->SetBackgroundColor(SK_ColorTRANSPARENT);
    web_contents()->Focus();
  }
}

}  // namespace chromecast
