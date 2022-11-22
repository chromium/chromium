// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/page_state_observer.h"

#include "base/memory/raw_ref.h"
#include "base/process/kill.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace cast_receiver {

class PageStateObserver::WebContentsObserverWrapper
    : public content::WebContentsObserver {
 public:
  using content::WebContentsObserver::Observe;

  WebContentsObserverWrapper(PageStateObserver& wrapped,
                             content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), wrapped_(wrapped) {}

  explicit WebContentsObserverWrapper(PageStateObserver& wrapped)
      : wrapped_(wrapped) {}

  ~WebContentsObserverWrapper() override { Observe(nullptr); }

 private:
  // content::WebContentsObserver implementation.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // This logic is a subset of that for DidFinishLoad() in
    // CastWebContentsImpl.
    int http_status_code = 0;
    content::NavigationEntry* nav_entry =
        web_contents()->GetController().GetVisibleEntry();
    if (nav_entry) {
      http_status_code = nav_entry->GetHttpStatusCode();
    }

    if (http_status_code != 0 && http_status_code / 100 != 2) {
      DLOG(WARNING) << "Stopping after receiving http failure status code: "
                    << http_status_code;
      wrapped_->OnPageStopped(StopReason::kHttpError,
                              net::ERR_HTTP_RESPONSE_CODE_FAILURE);
      return;
    }

    wrapped_->OnPageLoadComplete();
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    // This logic is a subset of that for DidFailLoad() in CastWebContentsImpl.
    if (render_frame_host->GetParent()) {
      DLOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
                  << ", error=" << error_code;
      return;
    }
    if (error_code == net::ERR_ABORTED) {
      // ERR_ABORTED means download was aborted by the app, typically this
      // happens when flinging URL for direct playback, the initial URLRequest
      // gets cancelled/aborted and then the same URL is requested via the
      // buffered data source for media::Pipeline playback.
      DLOG(WARNING) << "Load canceled: url=" << validated_url.spec();

      // We consider the page to be fully loaded in this case, since the app has
      // intentionally entered this state. If the app wanted to stop, it would
      // have called window.close() instead.
      wrapped_->OnPageLoadComplete();
      return;
    }

    wrapped_->OnPageStopped(StopReason::kHttpError,
                            static_cast<net::Error>(error_code));
  }

  void WebContentsDestroyed() override {
    content::WebContentsObserver::Observe(nullptr);
    wrapped_->OnPageStopped(StopReason::kApplicationRequest, net::OK);
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    content::WebContentsObserver::Observe(nullptr);
    wrapped_->OnPageStopped(StopReason::kHttpError, net::ERR_UNEXPECTED);
  }

  base::raw_ref<PageStateObserver> wrapped_;
};

PageStateObserver::PageStateObserver()
    : observer_wrapper_(std::make_unique<WebContentsObserverWrapper>(*this)) {}

PageStateObserver::PageStateObserver(content::WebContents* web_contents)
    : observer_wrapper_(
          std::make_unique<WebContentsObserverWrapper>(*this, web_contents)) {}

PageStateObserver::~PageStateObserver() = default;

void PageStateObserver::Observe(content::WebContents* web_contents) {
  observer_wrapper_->Observe(web_contents);
}

}  // namespace cast_receiver
