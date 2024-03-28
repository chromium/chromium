// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class SiteInstance;
}  // namespace content

namespace chromecast {

class CastWebService;
class RendererPrelauncher;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebViewDefault : public CastWebView,
                           private content::WebContentsDelegate {
 public:
  // |web_service| and |browser_context| should outlive this object. If
  // |cast_content_window| is not provided, an instance will be constructed from
  // |web_service|.
  CastWebViewDefault(
      mojom::CastWebViewParamsPtr params,
      CastWebService* web_service,
      content::BrowserContext* browser_context,
      std::unique_ptr<CastContentWindow> cast_content_window = nullptr);
  CastWebViewDefault(const CastWebViewDefault&) = delete;
  CastWebViewDefault& operator=(const CastWebViewDefault&) = delete;
  ~CastWebViewDefault() override;

  // CastWebView implementation:
  CastContentWindow* window() const override;
  content::WebContents* web_contents() const override;
  CastWebContents* cast_web_contents() override;
  base::TimeDelta shutdown_delay() const override;
  void OwnerDestroyed() override;

 private:
  // WebContentsDelegate implementation:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;

  mojom::CastWebViewParamsPtr params_;
  CastWebService* const web_service_;

  std::unique_ptr<RendererPrelauncher> renderer_prelauncher_;
  scoped_refptr<content::SiteInstance> site_instance_;
  std::unique_ptr<content::WebContents> web_contents_;
  CastWebContentsImpl cast_web_contents_;
  std::unique_ptr<CastContentWindow> window_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_DEFAULT_H_
