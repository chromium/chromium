// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/browser/cast_web_view.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class SiteInstance;
}  // namespace content

namespace chromecast {

class CastWebService;

class CastWebViewFactory : public CastWebView::Observer {
 public:
  explicit CastWebViewFactory(content::BrowserContext* browser_context);
  ~CastWebViewFactory() override;

  virtual std::unique_ptr<CastWebView> CreateWebView(
      const CastWebView::CreateParams& params,
      CastWebService* web_service,
      scoped_refptr<content::SiteInstance> site_instance,
      const GURL& initial_url);

  content::BrowserContext* browser_context() const { return browser_context_; }

 protected:
  // CastWebView::Observer implementation:
  void OnPageDestroyed(CastWebView* web_view) override;

  content::BrowserContext* const browser_context_;
  base::RepeatingCallback<void(CastWebView*, int)> register_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastWebViewFactory);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
