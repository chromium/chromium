// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/page_state_observer.h"

#include "base/memory/raw_ref.h"
#include "base/process/kill.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
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
  void TryCallOnPageStopped(StopReason reason, net::Error error_code) {
    if (!navigation_handle_) {
      return;
    }

    navigation_handle_ = nullptr;
    wrapped_->OnPageStopped(reason, error_code);
  }

  void TryCallOnPageLoadComplete() {
    if (!navigation_handle_) {
      return;
    }

    navigation_handle_ = nullptr;
    wrapped_->OnPageLoadComplete();
  }

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    navigation_handle_ = navigation_handle;
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Ignore sub-frame and non-current main frame navigation.
    if (navigation_handle != navigation_handle_) {
      return;
    }

    // If the navigation was not committed, it means either the page was a
    // download or error 204/205, or the navigation never left the previous
    // URL. Ignore these navigations.
    if (!navigation_handle->HasCommitted()) {
      LOG(WARNING) << "Navigation did not commit: url="
                   << navigation_handle->GetURL();
      navigation_handle_ = nullptr;
      return;
    }

    if (navigation_handle->IsErrorPage()) {
      const net::Error error_code = navigation_handle->GetNetErrorCode();
      LOG(ERROR) << "Got error on navigation: url="
                 << navigation_handle->GetURL() << ", error_code=" << error_code
                 << ", description=" << net::ErrorToShortString(error_code);
      TryCallOnPageStopped(StopReason::kHttpError, error_code);
      return;
    }

    // Notifies observers that the navigation of the main frame has finished
    // with no errors.
    TryCallOnPageLoadComplete();
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
      return;
    }

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
      TryCallOnPageStopped(StopReason::kHttpError,
                           net::ERR_HTTP_RESPONSE_CODE_FAILURE);
      return;
    }

    TryCallOnPageLoadComplete();
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
      TryCallOnPageLoadComplete();
      return;
    }

    TryCallOnPageStopped(StopReason::kHttpError,
                         static_cast<net::Error>(error_code));
  }

  void WebContentsDestroyed() override {
    content::WebContentsObserver::Observe(nullptr);
    TryCallOnPageStopped(StopReason::kApplicationRequest, net::OK);
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    content::WebContentsObserver::Observe(nullptr);
    TryCallOnPageStopped(StopReason::kHttpError, net::ERR_UNEXPECTED);
  }

  content::NavigationHandle* navigation_handle_ = nullptr;
  raw_ref<PageStateObserver> wrapped_;
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
