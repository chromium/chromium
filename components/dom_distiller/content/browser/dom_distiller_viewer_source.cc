// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/common/mojom/distiller_page_notifier_service.mojom.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/dom_distiller/core/viewer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

namespace dom_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source. Lifetime matches that of
// the current main frame's page in the Viewer instance.
class DomDistillerViewerSource::RequestViewerHandle
    : public DomDistillerRequestViewBase,
      public content::WebContentsObserver {
 public:
  RequestViewerHandle(content::WebContents* web_contents,
                      const GURL& expected_url,
                      DistilledPagePrefs* distilled_page_prefs);
  ~RequestViewerHandle() override;

  // content::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

 private:
  // Sends JavaScript to the attached Viewer, buffering data if the viewer isn't
  // ready.
  void SendJavaScript(const std::string& buffer) override;

  // Cancels the current view request. Once called, no updates will be
  // propagated to the view, and the request to DomDistillerService will be
  // cancelled.
  void Cancel();

  // The URL hosting the current view request;
  const GURL expected_url_;

  // Whether the page is sufficiently initialized to handle updates from the
  // distiller.
  bool waiting_for_page_ready_;

  // Temporary store of pending JavaScript if the page isn't ready to receive
  // data from distillation.
  std::string buffer_;
};

DomDistillerViewerSource::RequestViewerHandle::RequestViewerHandle(
    content::WebContents* web_contents,
    const GURL& expected_url,
    DistilledPagePrefs* distilled_page_prefs)
    : DomDistillerRequestViewBase(distilled_page_prefs),
      expected_url_(expected_url),
      waiting_for_page_ready_(true) {
  content::WebContentsObserver::Observe(web_contents);
  distilled_page_prefs_->AddObserver(this);
}

DomDistillerViewerSource::RequestViewerHandle::~RequestViewerHandle() {
  distilled_page_prefs_->RemoveObserver(this);
}

void DomDistillerViewerSource::RequestViewerHandle::SendJavaScript(
    const std::string& buffer) {
  if (waiting_for_page_ready_) {
    buffer_ += buffer;
  } else {
    DCHECK(buffer_.empty());
    if (web_contents()) {
      RunIsolatedJavaScript(web_contents()->GetMainFrame(), buffer);
    }
  }
}

void DomDistillerViewerSource::RequestViewerHandle::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  const GURL& navigation = navigation_handle->GetURL();
  bool expected_main_view_request = navigation == expected_url_;
  if (navigation_handle->IsSameDocument() || expected_main_view_request) {
    // In-page navigations, as well as the main view request can be ignored.
    if (expected_main_view_request) {
      content::RenderFrameHost* render_frame_host =
          navigation_handle->GetRenderFrameHost();
      CHECK_EQ(0, render_frame_host->GetEnabledBindings());

      // Tell the renderer that this is currently a distilled page.
      mojo::Remote<mojom::DistillerPageNotifierService> page_notifier_service;
      render_frame_host->GetRemoteInterfaces()->GetInterface(
          page_notifier_service.BindNewPipeAndPassReceiver());
      DCHECK(page_notifier_service);
      page_notifier_service->NotifyIsDistillerPage();
    }
    return;
  }

  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::RenderProcessGone(
    base::TerminationStatus status) {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::WebContentsDestroyed() {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::Cancel() {
  // No need to listen for notifications.
  content::WebContentsObserver::Observe(nullptr);

  // Schedule the Viewer for deletion. Ensures distillation is cancelled, and
  // any pending data stored in |buffer_| is released.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void DomDistillerViewerSource::RequestViewerHandle::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // DOMContentLoaded() is late enough to execute JavaScript, and is early
  // enough so that it's more likely that the title and content can be picked up
  // by TalkBack instead of the placeholder. If distillation is finished by
  // DOMContentLoaded(), onload() event would also be delayed, so that the
  // accessibility focus is more likely to be on the web content. Otherwise, the
  // focus is usually on the close button of the CustomTab (CCT), or nowhere. If
  // distillation finishes later than DOMContentLoaded(), or if for some
  // reason the accessibility focus is on the close button of the CCT, the title
  // could go unannounced.
  // See http://crbug.com/811417.
  if (render_frame_host->GetParent()) {
    return;
  }

  int64_t start_time_ms = url_utils::GetTimeFromDistillerUrl(
      render_frame_host->GetLastCommittedURL());
  if (start_time_ms > 0) {
    base::TimeTicks start_time =
        base::TimeDelta::FromMilliseconds(start_time_ms) + base::TimeTicks();
    base::TimeDelta latency = base::TimeTicks::Now() - start_time;

    UMA_HISTOGRAM_TIMES("DomDistiller.Time.ViewerLoading", latency);
  }

  // No SendJavaScript() calls allowed before |buffer_| is run and cleared.
  waiting_for_page_ready_ = false;
  if (!buffer_.empty()) {
    RunIsolatedJavaScript(web_contents()->GetMainFrame(), buffer_);
    buffer_.clear();
  }
  // No need to Cancel() here.
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& scheme)
    : scheme_(scheme), dom_distiller_service_(dom_distiller_service) {}

DomDistillerViewerSource::~DomDistillerViewerSource() {}

std::string DomDistillerViewerSource::GetSource() {
  return scheme_ + "://";
}

void DomDistillerViewerSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // TODO(crbug/1009127): simplify path matching.
  const std::string path = URLDataSource::URLToRequestPath(url);
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents)
    return;
  if (kViewerCssPath == path) {
    std::string css = viewer::GetCss();
    callback.Run(base::RefCountedString::TakeString(&css));
    return;
  }
  if (kViewerLoadingImagePath == path) {
    std::string image = viewer::GetLoadingImage();
    callback.Run(base::RefCountedString::TakeString(&image));
    return;
  }
  if (base::StartsWith(path, kViewerSaveFontScalingPath,
                       base::CompareCase::SENSITIVE)) {
    double scale = 1.0;
    if (base::StringToDouble(path.substr(strlen(kViewerSaveFontScalingPath)),
                             &scale)) {
      dom_distiller_service_->GetDistilledPagePrefs()->SetFontScaling(scale);
    }
  }

  // We need the host part to validate the parameter, but it's not available
  // from |URLDataSource|. |web_contents| is the most convenient place to
  // obtain the full URL.
  // TODO(crbug.com/991888): pass GURL in URLDataSource::StartDataRequest().
  const std::string query = GURL("https://host/" + path).query();
  GURL request_url = web_contents->GetVisibleURL();
  // The query should match what's seen in |web_contents|.
  // For javascript:window.open(), it's not the case, but it's not a supported
  // use case.
  if (request_url.query() != query || request_url.path() != "/") {
    request_url = GURL();
  }
  RequestViewerHandle* request_viewer_handle =
      new RequestViewerHandle(web_contents, request_url,
                              dom_distiller_service_->GetDistilledPagePrefs());
  std::unique_ptr<ViewerHandle> viewer_handle = viewer::CreateViewRequest(
      dom_distiller_service_, request_url, request_viewer_handle,
      web_contents->GetContainerBounds().size());

  GURL current_url(url_utils::GetOriginalUrlFromDistillerUrl(request_url));
  std::string unsafe_page_html = viewer::GetUnsafeArticleTemplateHtml(
      current_url.spec(),
      dom_distiller_service_->GetDistilledPagePrefs()->GetTheme(),
      dom_distiller_service_->GetDistilledPagePrefs()->GetFontFamily());

  if (viewer_handle) {
    // The service returned a |ViewerHandle| and guarantees it will call
    // the |RequestViewerHandle|, so passing ownership to it, to ensure the
    // request is not cancelled. The |RequestViewerHandle| will delete itself
    // after receiving the callback.
    request_viewer_handle->TakeViewerHandle(std::move(viewer_handle));
  } else {
    request_viewer_handle->FlagAsErrorPage();
  }

  // Place template on the page.
  callback.Run(base::RefCountedString::TakeString(&unsafe_page_html));
}

std::string DomDistillerViewerSource::GetMimeType(const std::string& path) {
  if (kViewerCssPath == path)
    return "text/css";
  if (kViewerLoadingImagePath == path)
    return "image/svg+xml";
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  return url.SchemeIs(scheme_);
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyStyleSrc() {
  return "style-src 'self' https://fonts.googleapis.com;";
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyChildSrc() {
  return "child-src *;";
}

}  // namespace dom_distiller
