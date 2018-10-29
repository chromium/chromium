// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_EXTENSION_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_EXTENSION_H_

#include <memory>

#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/cast_web_view.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace chromecast {

class CastExtensionHost;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebViewExtension : public CastWebView,
                             public content::WebContentsObserver {
 public:
  // |delegate| and |browser_context| should outlive the lifetime of this
  // object.
  CastWebViewExtension(const CreateParams& params,
                       content::BrowserContext* browser_context,
                       scoped_refptr<content::SiteInstance> site_instance,
                       const extensions::Extension* extension,
                       const GURL& initial_url);
  ~CastWebViewExtension() override;

  shell::CastContentWindow* window() const override;

  content::WebContents* web_contents() const override;

  // CastWebView implementation:
  void LoadUrl(GURL url) override;
  void ClosePage(const base::TimeDelta& shutdown_delay) override;
  void InitializeWindow(CastWindowManager* window_manager,
                        CastWindowManager::WindowId z_order,
                        VisibilityPriority initial_priority) override;
  void SetContext(base::Value context) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;

 private:
  // WebContentsObserver implementation:
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  Delegate* const delegate_;
  const std::unique_ptr<shell::CastContentWindow> window_;
  const std::unique_ptr<CastExtensionHost> extension_host_;
  CastWebContentsImpl cast_web_contents_;
  scoped_refptr<content::SiteInstance> site_instance_;

  DISALLOW_COPY_AND_ASSIGN(CastWebViewExtension);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_EXTENSION_H_
