// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chromecast/browser/cast_web_view.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromecast {

class CastWebService;

class CastWebViewFactory {
 public:
  explicit CastWebViewFactory(content::BrowserContext* browser_context);

  CastWebViewFactory(const CastWebViewFactory&) = delete;
  CastWebViewFactory& operator=(const CastWebViewFactory&) = delete;

  virtual ~CastWebViewFactory();

  virtual std::unique_ptr<CastWebView> CreateWebView(
      mojom::CastWebViewParamsPtr params,
      CastWebService* web_service);

  content::BrowserContext* browser_context() const { return browser_context_; }

 protected:
  content::BrowserContext* const browser_context_;
  base::RepeatingCallback<void(CastWebView*, int)> register_callback_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
