// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_
#define COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class BrowserContext;
}

namespace background_loader {

// This class maintains a WebContents used in the background. It can host
// a renderer but does not have any visible display.
class BackgroundLoaderContents : public content::WebContentsDelegate {
 public:
  // Delegate to help with decision making.
  class Delegate {
   public:
    // Decide whether single-file (e.g. pdf) downloads should be allowed to
    // start if a page initiates it. Callback can be called
    // synchronously. Returning true to the callback will initiate
    // the single file download. Assumes delegate will appropriately clean up.
    virtual void CanDownload(base::OnceCallback<void(bool)> callback) = 0;
  };

  // Creates BackgroundLoaderContents with specified |browser_context|. Uses
  // default session storage space.
  explicit BackgroundLoaderContents(content::BrowserContext* browser_context);

  BackgroundLoaderContents(const BackgroundLoaderContents&) = delete;
  BackgroundLoaderContents& operator=(const BackgroundLoaderContents&) = delete;

  ~BackgroundLoaderContents() override;

  // Loads the URL in a WebContents. Will call observe on all current observers
  // with the created WebContents.
  virtual void LoadPage(const GURL& url);
  // Cancels loading of the current page. Calls Close() on internal WebContents.
  virtual void Cancel();
  // Sets the delegate for this BackgroundLoaderContents; The delegate should
  // outlive the contents, therefore, raw pointers are allowed.
  void SetDelegate(Delegate* delegate);
  // Returns the inner web contents.
  content::WebContents* web_contents() { return web_contents_.get(); }

  // content::WebContentsDelegate implementation:
  bool IsNeverComposited(content::WebContents* web_contents) override;
  void CloseContents(content::WebContents* source) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash(content::WebContents* source) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;

  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;

#if BUILDFLAG(IS_ANDROID)
  bool ShouldBlockMediaRequest(const GURL& url) override;
#endif

  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool ShouldAllowLazyLoad() override;

 private:
  friend class BackgroundLoaderContentsTest;
  friend class BackgroundLoaderContentsStub;

  BackgroundLoaderContents();

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace background_loader
#endif  // COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_
