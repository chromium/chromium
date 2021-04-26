// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_PAGE_WEB_CONTENTS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_PAGE_WEB_CONTENTS_H_

#include <memory>
#include <string>

#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace dom_distiller {

class SourcePageHandleWebContents : public SourcePageHandle {
 public:
  SourcePageHandleWebContents(content::WebContents* web_contents, bool owned);
  ~SourcePageHandleWebContents() override;

  // Retreives the WebContents. The SourcePageHandleWebContents keeps ownership.
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // The WebContents this class holds.
  content::WebContents* web_contents_;
  // Whether this owns |web_contents_|.
  bool owned_;
};

class DistillerPageWebContentsFactory : public DistillerPageFactory {
 public:
  explicit DistillerPageWebContentsFactory(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~DistillerPageWebContentsFactory() override = default;

  std::unique_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& render_view_size) const override;
  std::unique_ptr<DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) const override;

 private:
  content::BrowserContext* browser_context_;
};

class DistillerPageWebContents : public DistillerPage,
                                 public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  DistillerPageWebContents(content::BrowserContext* browser_context,
                           const gfx::Size& render_view_size,
                           std::unique_ptr<SourcePageHandleWebContents>
                               optional_web_contents_handle);
  ~DistillerPageWebContents() override;

  // content::WebContentsDelegate implementation.
  gfx::Size GetSizeForNewRenderView(
      content::WebContents* web_contents) override;

  // content::WebContentsObserver implementation.
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;

  DistillerPageWebContents(const DistillerPageWebContents&) = delete;
  DistillerPageWebContents& operator=(const DistillerPageWebContents&) = delete;

 protected:
  bool StringifyOutput() override;
  void DistillPageImpl(const GURL& url, const std::string& script) override;

 private:
  friend class TestDistillerPageWebContents;

  enum State {
    // The page distiller is idle.
    IDLE,
    // A page is currently loading.
    LOADING_PAGE,
    // There was an error processing the page.
    PAGELOAD_FAILED,
    // JavaScript is executing within the context of the page. When the
    // JavaScript completes, the state will be returned to |IDLE|.
    EXECUTING_JAVASCRIPT
  };

  // Creates a new WebContents, adds |this| as an observer, and loads the
  // |url|.
  virtual void CreateNewWebContents(const GURL& url);

  // Injects and executes JavaScript in the context of a loaded page. This
  // must only be called after the page has successfully loaded.
  void ExecuteJavaScript();

  // Called when the distillation is done or if the page load failed.
  void OnWebContentsDistillationDone(const GURL& page_url,
                                     const base::TimeTicks& javascript_start,
                                     base::Value value);

  // The current state of the |DistillerPage|, initially |IDLE|.
  State state_;

  // The JavaScript to inject to extract content.
  std::string script_;

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle_;

  content::BrowserContext* browser_context_;
  gfx::Size render_view_size_;
  base::WeakPtrFactory<DistillerPageWebContents> weak_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_PAGE_WEB_CONTENTS_H_
