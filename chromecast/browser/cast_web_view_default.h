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
class SiteInstance;
}  // namespace content

namespace chromecast {

class CastWebService;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebViewDefault : public CastWebView,
                           content::WebContentsObserver,
                           content::WebContentsDelegate {
 public:
  // |web_service| and |browser_context| should outlive this object. If
  // |cast_content_window| is not provided, an instance will be constructed from
  // |web_service|.
  CastWebViewDefault(
      const CreateParams& params,
      CastWebService* web_service,
      content::BrowserContext* browser_context,
      scoped_refptr<content::SiteInstance> site_instance,
      std::unique_ptr<CastContentWindow> cast_content_window = nullptr);
  ~CastWebViewDefault() override;

  // CastWebView implementation:
  CastContentWindow* window() const override;
  content::WebContents* web_contents() const override;
  CastWebContents* cast_web_contents() override;
  void LoadUrl(GURL url) override;
  void ClosePage() override;
  void InitializeWindow(mojom::ZOrder z_order,
                        VisibilityPriority initial_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;

 private:
  // WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // WebContentsDelegate implementation:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
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
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;

  CastWebService* const web_service_;
  content::BrowserContext* const browser_context_;
  const scoped_refptr<content::SiteInstance> site_instance_;

  const std::string activity_id_;
  const std::string session_id_;
  const std::string sdk_version_;
  const bool allow_media_access_;
  const std::string log_prefix_;

  std::unique_ptr<content::WebContents> web_contents_;
  CastWebContentsImpl cast_web_contents_;
  std::unique_ptr<CastContentWindow> window_;
  bool resize_window_when_navigation_starts_;

  DISALLOW_COPY_AND_ASSIGN(CastWebViewDefault);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_
