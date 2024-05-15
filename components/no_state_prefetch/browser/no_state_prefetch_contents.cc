// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"

#include <stddef.h>

#include <functional>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"
#include "components/no_state_prefetch/common/render_frame_prerender_messages.mojom.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/http/http_response_headers.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

class NoStatePrefetchContentsFactoryImpl
    : public NoStatePrefetchContents::Factory {
 public:
  NoStatePrefetchContents* CreateNoStatePrefetchContents(
      std::unique_ptr<NoStatePrefetchContentsDelegate> delegate,
      NoStatePrefetchManager* no_state_prefetch_manager,
      content::BrowserContext* browser_context,
      const GURL& url,
      const content::Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      Origin origin) override {
    return new NoStatePrefetchContents(
        std::move(delegate), no_state_prefetch_manager, browser_context, url,
        referrer, initiator_origin, origin);
  }
};

void SetPreloadingTriggeringOutcome(
    content::PreloadingAttempt* attempt,
    content::PreloadingTriggeringOutcome outcome) {
  if (!attempt)
    return;

  attempt->SetTriggeringOutcome(outcome);
}

content::PreloadingFailureReason ToPreloadingFailureReason(FinalStatus status) {
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

// WebContentsDelegateImpl -----------------------------------------------------

class NoStatePrefetchContents::WebContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegateImpl(
      NoStatePrefetchContents* no_state_prefetch_contents)
      : no_state_prefetch_contents_(no_state_prefetch_contents) {}

  // content::WebContentsDelegate implementation:
  WebContents* OpenURLFromTab(
      WebContents* source,
      const OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    // |OpenURLFromTab| is typically called when a frame performs a navigation
    // that requires the browser to perform the transition instead of WebKit.
    // Examples include client redirects to hosted app URLs.
    // TODO(cbentzel): Consider supporting this for CURRENT_TAB dispositions, if
    // it is a common case during prerenders.
    no_state_prefetch_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return NULL;
  }

  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation) override {
    // Cancel the prerender if the navigation attempts to transfer to a
    // different process.  Examples include server redirects to privileged pages
    // or cross-site subframe navigations in --site-per-process.
    no_state_prefetch_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return false;
  }

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override {
    no_state_prefetch_contents_->Destroy(FINAL_STATUS_DOWNLOAD);
    // Cancel the download.
    std::move(callback).Run(false);
  }

  bool OnGoToEntryOffset(int offset) override {
    // This isn't allowed because the history merge operation
    // does not work if there are renderer issued challenges.
    // TODO(cbentzel): Cancel in this case? May not need to do
    // since render-issued offset navigations are not guaranteed,
    // but indicates that the page cares about the history.
    return false;
  }

  gfx::Size GetSizeForNewRenderView(WebContents* web_contents) override {
    // Have to set the size of the RenderView on initialization to be sure it is
    // set before the RenderView is hidden on all platforms (esp. Android).
    return no_state_prefetch_contents_->bounds_.size();
  }

  void CapturePaintPreviewOfSubframe(
      WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      RenderFrameHost* render_frame_host) override {
    auto* client =
        paint_preview::PaintPreviewClient::FromWebContents(web_contents);
    if (client)
      client->CaptureSubframePaintPreview(guid, rect, render_frame_host);
  }

 private:
  raw_ptr<NoStatePrefetchContents> no_state_prefetch_contents_;
};

NoStatePrefetchContents::Observer::~Observer() {}

NoStatePrefetchContents::NoStatePrefetchContents(
    std::unique_ptr<NoStatePrefetchContentsDelegate> delegate,
    NoStatePrefetchManager* no_state_prefetch_manager,
    content::BrowserContext* browser_context,
    const GURL& url,
    const content::Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    Origin origin)
    : no_state_prefetch_manager_(no_state_prefetch_manager),
      delegate_(std::move(delegate)),
      prefetch_url_(url),
      referrer_(referrer),
      initiator_origin_(initiator_origin),
      browser_context_(browser_context),
      final_status_(FINAL_STATUS_UNKNOWN),
      process_pid_(base::kNullProcessId),
      origin_(origin) {
  switch (origin) {
    case ORIGIN_NAVIGATION_PREDICTOR:
      DCHECK(!initiator_origin_.has_value());
      break;

    case ORIGIN_GWS_PRERENDER:
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
    case ORIGIN_LINK_REL_NEXT:
    case ORIGIN_SAME_ORIGIN_SPECULATION:
      DCHECK(initiator_origin_.has_value());
      break;
    case ORIGIN_NONE:
    case ORIGIN_MAX:
      NOTREACHED_IN_MIGRATION();
  }

  DCHECK(no_state_prefetch_manager);
}

bool NoStatePrefetchContents::Init() {
  return AddAliasURL(prefetch_url_);
}

// static
NoStatePrefetchContents::Factory* NoStatePrefetchContents::CreateFactory() {
  return new NoStatePrefetchContentsFactoryImpl();
}

void NoStatePrefetchContents::SetPreloadingFailureReason(FinalStatus status) {
  if (!attempt_)
    return;

  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_NOSTATE_PREFETCH_FINISHED:
      // When adding a new failure reason, consider whether it should be
      // propagated to `attempt_`. Most values should be propagated, but we
      // explicitly do not propagate failure reasons if:
      // the no state prefetch was actually successful (USED OR
      // PREFETCH_FINISHED).
      return;
    case FINAL_STATUS_TIMED_OUT:
    case FINAL_STATUS_PROFILE_DESTROYED:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_AUTH_NEEDED:
    case FINAL_STATUS_DOWNLOAD:
    case FINAL_STATUS_MEMORY_LIMIT_EXCEEDED:
    case FINAL_STATUS_TOO_MANY_PROCESSES:
    case FINAL_STATUS_RATE_LIMIT_EXCEEDED:
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_UNSUPPORTED_SCHEME:
    case FINAL_STATUS_RECENTLY_VISITED:
    case FINAL_STATUS_SAFE_BROWSING:
    case FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    case FINAL_STATUS_CANCELLED:
    case FINAL_STATUS_SSL_ERROR:
    case FINAL_STATUS_DUPLICATE:
    case FINAL_STATUS_OPEN_URL:
    case FINAL_STATUS_NAVIGATION_INTERCEPTED:
    case FINAL_STATUS_PRERENDERING_DISABLED:
    case FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES:
    case FINAL_STATUS_LOW_END_DEVICE:
    case FINAL_STATUS_BROWSER_SWITCH:
    case FINAL_STATUS_GWS_HOLDBACK:
    case FINAL_STATUS_UNKNOWN:
    case FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK:
    case FINAL_STATUS_SINGLE_PROCESS:
    case FINAL_STATUS_LINK_REL_NEXT_NOT_ALLOWED:
    case FINAL_STATUS_PREFETCH_HOLDBACK:
    case FINAL_STATUS_MAX:
      attempt_->SetFailureReason(ToPreloadingFailureReason(status));
      // We reset the attempt to ensure we don't update once we have reported it
      // as failure or accidentally use it for any other prerender attempts as
      // PrerenderHost deletion is async.
      attempt_.reset();
      return;
  }
}

void NoStatePrefetchContents::StartPrerendering(
    const gfx::Rect& bounds,
    SessionStorageNamespace* session_storage_namespace,
    base::WeakPtr<content::PreloadingAttempt> attempt) {
  DCHECK(browser_context_);
  DCHECK(!bounds.IsEmpty());
  DCHECK(!prefetching_has_started_);
  DCHECK(!no_state_prefetch_contents_);
  DCHECK_EQ(1U, alias_urls_.size());

  if (session_storage_namespace)
    session_storage_namespace_id_ = session_storage_namespace->id();
  bounds_ = bounds;

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  prefetching_has_started_ = true;
  attempt_ = std::move(attempt);
  SetPreloadingTriggeringOutcome(
      attempt_.get(), content::PreloadingTriggeringOutcome::kRunning);

  no_state_prefetch_contents_ = CreateWebContents(session_storage_namespace);
  no_state_prefetch_contents_->SetOwnerLocationForDebug(FROM_HERE);
  content::WebContentsObserver::Observe(no_state_prefetch_contents_.get());
  delegate_->OnNoStatePrefetchContentsCreated(
      no_state_prefetch_contents_.get());

  web_contents_delegate_ = std::make_unique<WebContentsDelegateImpl>(this);
  no_state_prefetch_contents_->SetDelegate(web_contents_delegate_.get());

  // Set the size of the prerender WebContents.
  no_state_prefetch_contents_->Resize(bounds_);
  no_state_prefetch_contents_->WasHidden();

  // TODO(davidben): This logic assumes each prerender has at most one
  // process. https://crbug.com/440544
  no_state_prefetch_manager()->AddPrerenderProcessHost(
      GetPrimaryMainFrame()->GetProcess());

  NotifyPrefetchStart();

  content::NavigationController::LoadURLParams load_url_params(prefetch_url_);
  load_url_params.referrer = referrer_;
  load_url_params.initiator_origin = initiator_origin_;
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  if (origin_ == ORIGIN_NAVIGATION_PREDICTOR) {
    load_url_params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED);
  }
  no_state_prefetch_contents_->GetController().LoadURLWithParams(
      load_url_params);
}

void NoStatePrefetchContents::SetFinalStatus(FinalStatus final_status) {
  DCHECK_GE(final_status, FINAL_STATUS_USED);
  DCHECK_LT(final_status, FINAL_STATUS_MAX);

  DCHECK_EQ(FINAL_STATUS_UNKNOWN, final_status_);

  final_status_ = final_status;

  SetPreloadingFailureReason(final_status);
}

NoStatePrefetchContents::~NoStatePrefetchContents() {
  DCHECK_NE(FINAL_STATUS_UNKNOWN, final_status());
  DCHECK(prefetching_has_been_cancelled() ||
         final_status() == FINAL_STATUS_USED);
  DCHECK_NE(ORIGIN_MAX, origin());

  no_state_prefetch_manager_->RecordFinalStatus(origin(), final_status());

  if (no_state_prefetch_contents_) {
    no_state_prefetch_contents_->SetDelegate(nullptr);
    content::WebContentsObserver::Observe(nullptr);
    delegate_->ReleaseNoStatePrefetchContents(
        no_state_prefetch_contents_.get());
  }
}

void NoStatePrefetchContents::AddObserver(Observer* observer) {
  DCHECK_EQ(FINAL_STATUS_UNKNOWN, final_status_);
  observer_list_.AddObserver(observer);
}

void NoStatePrefetchContents::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<WebContents> NoStatePrefetchContents::CreateWebContents(
    SessionStorageNamespace* session_storage_namespace) {
  // TODO(ajwong): Remove the temporary map once prerendering is aware of
  // multiple session storage namespaces per tab.
  return WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(browser_context_),
      CreateMapWithDefaultSessionStorageNamespace(browser_context_,
                                                  session_storage_namespace));
}

void NoStatePrefetchContents::NotifyPrefetchStart() {
  DCHECK_EQ(FINAL_STATUS_UNKNOWN, final_status_);
  for (Observer& observer : observer_list_)
    observer.OnPrefetchStart(this);
}

void NoStatePrefetchContents::NotifyPrefetchStopLoading() {
  // Set the status to Ready once the prefetch stops loading. For
  // NoStatePrefetch we don't know if the final resource was used from cache
  // later on or not. kReady doesn't mean it is a success.
  SetPreloadingTriggeringOutcome(attempt_.get(),
                                 content::PreloadingTriggeringOutcome::kReady);
  for (Observer& observer : observer_list_)
    observer.OnPrefetchStopLoading(this);
}

void NoStatePrefetchContents::NotifyPrefetchStop() {
  DCHECK_NE(FINAL_STATUS_UNKNOWN, final_status_);
  for (Observer& observer : observer_list_)
    observer.OnPrefetchStop(this);
  observer_list_.Clear();
}

bool NoStatePrefetchContents::CheckURL(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }
  if (no_state_prefetch_manager_->HasRecentlyBeenNavigatedTo(origin(), url)) {
    Destroy(FINAL_STATUS_RECENTLY_VISITED);
    return false;
  }
  return true;
}

bool NoStatePrefetchContents::AddAliasURL(const GURL& url) {
  if (!CheckURL(url))
    return false;

  alias_urls_.push_back(url);
  return true;
}

bool NoStatePrefetchContents::Matches(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace) const {
  // TODO(davidben): Remove any consumers that pass in a NULL
  // session_storage_namespace and only test with matches.
  if (session_storage_namespace &&
      session_storage_namespace_id_ != session_storage_namespace->id()) {
    return false;
  }
  return base::Contains(alias_urls_, url);
}

void NoStatePrefetchContents::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    // The renderer process is deliberately terminated (e.g. browser or test
    // shutdown) before the termination notification is received.
    Destroy(FINAL_STATUS_APP_TERMINATING);
  }
  Destroy(FINAL_STATUS_RENDERER_CRASHED);
}

void NoStatePrefetchContents::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // When a new RenderFrame is created for a prerendering WebContents, tell the
  // new RenderFrame it's being used for prerendering before any navigations
  // occur.  Note that this is always triggered before the first navigation, so
  // there's no need to send the message just after the WebContents is created.
  mojo::AssociatedRemote<prerender::mojom::PrerenderMessages>
      prerender_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &prerender_render_frame);
  prerender_render_frame->SetIsPrerendering(
      PrerenderHistograms::GetHistogramPrefix(origin_));
}

void NoStatePrefetchContents::DidStopLoading() {
  NotifyPrefetchStopLoading();
}

void NoStatePrefetchContents::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!CheckURL(navigation_handle->GetURL()))
    return;

  // Usually, this event fires if the user clicks or enters a new URL.
  // Neither of these can happen in the case of an invisible prerender.
  // So the cause is: Some JavaScript caused a new URL to be loaded.  In that
  // case, the spinner would start again in the browser, so we must reset
  // has_finished_loading_ so that the spinner won't be stopped.
  has_finished_loading_ = false;
}

void NoStatePrefetchContents::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // If it's a redirect on the top-level resource, the name needs to be
  // remembered for future matching, and if it redirects to an https resource,
  // it needs to be canceled. If a subresource is redirected, nothing changes.
  CheckURL(navigation_handle->GetURL());
}

void NoStatePrefetchContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame())
    has_finished_loading_ = true;
}

void NoStatePrefetchContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (navigation_handle->IsErrorPage()) {
    // Maintain same behavior as old navigation API when the URL is unreachable
    // and leads to an error page. Also maintain same final status code that
    // previous navigation API returned, which was reached because the URL for
    // the error page was kUnreachableWebDataURL and that was interpreted as
    // unsupported scheme.
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return;
  }

  // Add each redirect as an alias. |navigation_handle->GetURL()| is included in
  // |navigation_handle->GetRedirectChain()|.
  //
  // TODO(davidben): We do not correctly patch up history for renderer-initated
  // navigations which add history entries. http://crbug.com/305660.
  for (const auto& redirect : navigation_handle->GetRedirectChain()) {
    if (!AddAliasURL(redirect))
      return;
  }
}

void NoStatePrefetchContents::Destroy(FinalStatus final_status) {
  DCHECK_NE(final_status, FINAL_STATUS_USED);

  if (prefetching_has_been_cancelled_) {
    return;
  }

  SetFinalStatus(final_status);

  prefetching_has_been_cancelled_ = true;
  no_state_prefetch_manager_->AddToHistory(this);
  no_state_prefetch_manager_->SetPrefetchFinalStatusForUrl(prefetch_url_,
                                                           final_status);
  no_state_prefetch_manager_->MoveEntryToPendingDelete(this, final_status);

  if (prefetching_has_started()) {
    NotifyPrefetchStop();
  }
}

void NoStatePrefetchContents::DestroyWhenUsingTooManyResources() {
  if (process_pid_ == base::kNullProcessId) {
    RenderFrameHost* rfh = GetPrimaryMainFrame();
    if (!rfh)
      return;

    content::RenderProcessHost* rph = rfh->GetProcess();
    if (!rph)
      return;

    base::ProcessHandle handle = rph->GetProcess().Handle();
    if (handle == base::kNullProcessHandle)
      return;

    process_pid_ = rph->GetProcess().Pid();
  }

  if (process_pid_ == base::kNullProcessId)
    return;

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          process_pid_,
          base::BindOnce(&NoStatePrefetchContents::DidGetMemoryUsage,
                         weak_factory_.GetWeakPtr()));
}

void NoStatePrefetchContents::DidGetMemoryUsage(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump) {
  if (!success)
    return;

  for (const memory_instrumentation::GlobalMemoryDump::ProcessDump& dump :
       global_dump->process_dumps()) {
    if (dump.pid() != process_pid_)
      continue;

    // If |final_status_| == |FINAL_STATUS_USED|, then destruction will be
    // handled by the entity that set final_status_.
    if (dump.os_dump().private_footprint_kb * 1024 >
            no_state_prefetch_manager_->config().max_bytes &&
        final_status_ != FINAL_STATUS_USED) {
      Destroy(FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
    }
    return;
  }
}

RenderFrameHost* NoStatePrefetchContents::GetPrimaryMainFrame() {
  return no_state_prefetch_contents_
             ? no_state_prefetch_contents_->GetPrimaryMainFrame()
             : nullptr;
}

std::optional<base::Value::Dict> NoStatePrefetchContents::GetAsDict() const {
  if (!no_state_prefetch_contents_)
    return std::nullopt;
  base::Value::Dict dict;
  dict.Set("url", prefetch_url_.spec());
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta duration = current_time - load_start_time_;
  dict.Set("duration", static_cast<int>(duration.InSeconds()));
  dict.Set("is_loaded", no_state_prefetch_contents_ &&
                            !no_state_prefetch_contents_->IsLoading());
  return dict;
}

void NoStatePrefetchContents::MarkAsUsedForTesting() {
  SetFinalStatus(FINAL_STATUS_USED);
  NotifyPrefetchStop();
}

void NoStatePrefetchContents::CancelPrerenderForUnsupportedScheme() {
  Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
}

void NoStatePrefetchContents::CancelPrerenderForNoStatePrefetch() {
  Destroy(FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
}

void NoStatePrefetchContents::AddPrerenderCancelerReceiver(
    mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver) {
  prerender_canceler_receiver_set_.Add(this, std::move(receiver));
}

}  // namespace prerender
