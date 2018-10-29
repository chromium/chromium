// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/cast_web_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
class RenderViewHost;
class SiteInstance;
}  // namespace content

namespace chromecast {

class CastWebContentsManager;
class CastWindowManager;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebViewDefault : public CastWebView,
                           content::WebContentsObserver,
                           content::WebContentsDelegate {
 public:
  // |web_contents_manager| and |browser_context| should outlive this object.
  CastWebViewDefault(const CreateParams& params,
                     CastWebContentsManager* web_contents_manager,
                     content::BrowserContext* browser_context,
                     scoped_refptr<content::SiteInstance> site_instance);
  ~CastWebViewDefault() override;

  // CastWebView implementation:
  shell::CastContentWindow* window() const override;
  content::WebContents* web_contents() const override;
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
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;

  // WebContentsDelegate implementation:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;

  CastWebContentsManager* const web_contents_manager_;
  content::BrowserContext* const browser_context_;
  const scoped_refptr<content::SiteInstance> site_instance_;

  Delegate* const delegate_;
  const bool transparent_;
  const bool allow_media_access_;

  std::unique_ptr<content::WebContents> web_contents_;
  CastWebContentsImpl cast_web_contents_;
  std::unique_ptr<shell::CastContentWindow> window_;
  bool did_start_navigation_;
  base::TimeDelta shutdown_delay_;

  DISALLOW_COPY_AND_ASSIGN(CastWebViewDefault);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_
