// Copyright 2018 The Chromium Authors
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

std::unique_ptr<CastWebView> CastWebViewFactory::CreateWebView(
    mojom::CastWebViewParamsPtr params,
    CastWebService* web_service) {
  return std::make_unique<CastWebViewDefault>(std::move(params), web_service,
                                              browser_context_);
}

}  // namespace chromecast
