// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSION_PAGE_H_
#define CHROMECAST_BROWSER_EXTENSION_PAGE_H_

#include <memory>

#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {

class CastContentWindowAura;
class CastExtensionHost;
class CastWindowManager;

// A simplified interface for loading and displaying WebContents in cast_shell.
class ExtensionPage : public content::WebContentsObserver {
 public:
  ExtensionPage(const CastWebContents::InitParams& init_params,
                const CastContentWindow::CreateParams& window_params,
                std::unique_ptr<CastExtensionHost> extension_host,
                CastWindowManager* window_manager);
  ~ExtensionPage() override;

  content::WebContents* web_contents() const;
  CastWebContents* cast_web_contents();

  void Launch();
  void InitializeWindow();

 private:
  // WebContentsObserver implementation:
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  const std::unique_ptr<CastContentWindowAura> window_;
  const std::unique_ptr<CastExtensionHost> extension_host_;
  CastWebContentsImpl cast_web_contents_;
  scoped_refptr<content::SiteInstance> site_instance_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPage);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_EXTENSION_PAGE_H_
