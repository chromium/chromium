// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/dom_distiller/core/viewer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
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
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
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
      RunIsolatedJavaScript(web_contents()->GetPrimaryMainFrame(), buffer);
    }
  }
}

void DomDistillerViewerSource::RequestViewerHandle::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted())
    return;

  const GURL& navigation = navigation_handle->GetURL();
  bool expected_main_view_request = navigation == expected_url_;
  if (navigation_handle->IsSameDocument() || expected_main_view_request) {
    // In-page navigations, as well as the main view request can be ignored.
    return;
  }

  // At the moment we destroy the handle and won't be able
  // to restore the document later, so we prevent the page
  // from being stored in back-forward cache.
  content::BackForwardCache::DisableForRenderFrameHost(
      navigation_handle->GetPreviousRenderFrameHostId(),
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kDomDistillerViewerSource));

  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::
    PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) {
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
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
  if (render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  // No SendJavaScript() calls allowed before |buffer_| is run and cleared.
  waiting_for_page_ready_ = false;
  if (!buffer_.empty()) {
    RunIsolatedJavaScript(web_contents()->GetPrimaryMainFrame(), buffer_);
    buffer_.clear();
  }
  // No need to Cancel() here.
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerServiceInterface* dom_distiller_service)
    : scheme_(kDomDistillerScheme),
      dom_distiller_service_(dom_distiller_service) {}

DomDistillerViewerSource::~DomDistillerViewerSource() = default;

std::string DomDistillerViewerSource::GetSource() {
  return scheme_ + "://";
}

void DomDistillerViewerSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug.com/40050262): simplify path matching.
  const std::string path = URLDataSource::URLToRequestPath(url);
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents)
    return;
#if !BUILDFLAG(IS_ANDROID)
  // Don't allow loading of mixed content on Reader Mode pages.
  blink::web_pref::WebPreferences prefs =
      web_contents->GetOrCreateWebPreferences();
  prefs.strict_mixed_content_checking = true;
  web_contents->SetWebPreferences(prefs);
#endif  // !BUILDFLAG(IS_ANDROID)
  if (kViewerCssPath == path) {
    std::string css = viewer::GetCss();
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(css)));
    return;
  }
  if (kViewerLoadingImagePath == path) {
    std::string image = viewer::GetLoadingImage();
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(image)));
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
  // TODO(crbug.com/40095934): pass GURL in URLDataSource::StartDataRequest().
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

  // Pass an empty nonce value as the CSP is only inlined on the iOS build.
  std::string unsafe_page_html = viewer::GetArticleTemplateHtml(
      dom_distiller_service_->GetDistilledPagePrefs()->GetTheme(),
      dom_distiller_service_->GetDistilledPagePrefs()->GetFontFamily(),
      std::string());

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
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      std::move(unsafe_page_html)));
}

std::string DomDistillerViewerSource::GetMimeType(const GURL& url) {
  const std::string_view path = url.path_piece().substr(1);
  if (kViewerCssPath == path)
    return "text/css";
  if (kViewerLoadingImagePath == path)
    return "image/svg+xml";
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  return url.SchemeIs(scheme_);
}

std::string DomDistillerViewerSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  if (directive == network::mojom::CSPDirectiveName::StyleSrc) {
    return "style-src 'self' https://fonts.googleapis.com;";
  } else if (directive == network::mojom::CSPDirectiveName::ChildSrc) {
    return "child-src *;";
  } else if (directive ==
                 network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
             directive == network::mojom::CSPDirectiveName::TrustedTypes) {
    // This removes require-trusted-types-for and trusted-types directives
    // from the CSP header.
    return std::string();
  }

  return content::URLDataSource::GetContentSecurityPolicy(directive);
}

}  // namespace dom_distiller
