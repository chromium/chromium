// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_extension.h"

#include "base/logging.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_extension_host.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_system.h"
#include "net/base/net_errors.h"
#include "ui/aura/window.h"

namespace chromecast {

CastWebViewExtension::CastWebViewExtension(
    const CreateParams& params,
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance,
    const extensions::Extension* extension,
    const GURL& initial_url)
    : delegate_(params.delegate),
      window_(shell::CastContentWindow::Create(params.window_params)),
      extension_host_(std::make_unique<CastExtensionHost>(
          browser_context,
          params.delegate,
          extension,
          initial_url,
          site_instance.get(),
          extensions::VIEW_TYPE_EXTENSION_POPUP)),
      cast_web_contents_(delegate_,
                         extension_host_->host_contents(),
                         params.enabled_for_dev) {
  DCHECK(delegate_);
  content::WebContentsObserver::Observe(web_contents());
  web_contents()->GetNativeView()->SetName(params.activity_id);
}

CastWebViewExtension::~CastWebViewExtension() {
  content::WebContentsObserver::Observe(nullptr);
}

shell::CastContentWindow* CastWebViewExtension::window() const {
  return window_.get();
}

content::WebContents* CastWebViewExtension::web_contents() const {
  return extension_host_->host_contents();
}

void CastWebViewExtension::LoadUrl(GURL url) {
  extension_host_->CreateRenderViewSoon();
}

// Extension web view cannot be closed deliberately.
void CastWebViewExtension::ClosePage(const base::TimeDelta& shutdown_delay) {}

void CastWebViewExtension::InitializeWindow(
    CastWindowManager* window_manager,
    CastWindowManager::WindowId z_order,
    VisibilityPriority initial_priority) {
  window_->CreateWindowForWebContents(web_contents(), window_manager, z_order,
                                      initial_priority);
  web_contents()->Focus();
}

void CastWebViewExtension::SetContext(base::Value context) {}

void CastWebViewExtension::GrantScreenAccess() {
  window_->GrantScreenAccess();
}

void CastWebViewExtension::RevokeScreenAccess() {
  window_->RevokeScreenAccess();
}

void CastWebViewExtension::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

}  // namespace chromecast
