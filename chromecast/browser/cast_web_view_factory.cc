// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_factory.h"

#include "chromecast/browser/cast_web_view_default.h"
#include "chromecast/chromecast_buildflags.h"

namespace chromecast {

CastWebViewFactory::CastWebViewFactory(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

CastWebViewFactory::~CastWebViewFactory() = default;

void CastWebViewFactory::OnPageDestroyed(CastWebView* web_view) {
  web_view->RemoveObserver(this);
}

std::unique_ptr<CastWebView> CastWebViewFactory::CreateWebView(
    const CastWebView::CreateParams& params,
    CastWebService* web_service,
    scoped_refptr<content::SiteInstance> site_instance,
    const GURL& initial_url) {
  std::unique_ptr<CastWebView> webview;
  webview = std::make_unique<CastWebViewDefault>(
      params, web_service, browser_context_, site_instance);
  if (webview) {
    webview->AddObserver(this);
  }
  return webview;
}

}  // namespace chromecast
