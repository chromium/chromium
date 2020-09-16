// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_manager.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/prerender/browser/prerender_field_trial.h"
#include "components/prerender/browser/prerender_handle.h"
#include "components/prerender/browser/prerender_histograms.h"
#include "components/prerender/browser/prerender_history.h"
#include "components/prerender/browser/prerender_manager_delegate.h"
#include "components/prerender/browser/prerender_util.h"
#include "components/prerender/common/prerender_final_status.h"
#include "components/prerender/common/prerender_types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "ui/gfx/geometry/rect.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

// Time interval at which periodic cleanups are performed.
constexpr base::TimeDelta kPeriodicCleanupInterval =
    base::TimeDelta::FromMilliseconds(1000);

// Time interval after which OnCloseWebContentsDeleter will schedule a
// WebContents for deletion.
constexpr base::TimeDelta kDeleteWithExtremePrejudice =
    base::TimeDelta::FromSeconds(3);

// Length of prerender history, for display in chrome://net-internals
constexpr int kHistoryLength = 100;

}  // namespace

class PrerenderManager::OnCloseWebContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseWebContentsDeleter> {
 public:
  OnCloseWebContentsDeleter(PrerenderManager* manager,
                            std::unique_ptr<WebContents> tab)
      : manager_(manager), tab_(std::move(tab)) {
    tab_->SetDelegate(this);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &OnCloseWebContentsDeleter::ScheduleWebContentsForDeletion,
            AsWeakPtr(), /*timeout=*/true),
        kDeleteWithExtremePrejudice);
  }

  void CloseContents(WebContents* source) override {
    DCHECK_EQ(tab_.get(), source);
    ScheduleWebContentsForDeletion(/*timeout=*/false);
  }

 private:
  void ScheduleWebContentsForDeletion(bool timeout) {
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
    tab_->SetDelegate(nullptr);
    manager_->ScheduleDeleteOldWebContents(std::move(tab_), this);
    // |this| is deleted at this point.
  }

  PrerenderManager* const manager_;
  std::unique_ptr<WebContents> tab_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseWebContentsDeleter);
};

PrerenderManagerObserver::~PrerenderManagerObserver() = default;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_NOSTATE_PREFETCH;

struct PrerenderManager::NavigationRecord {
  NavigationRecord(const GURL& url, base::TimeTicks time, Origin origin)
      : url(url), time(time), origin(origin) {}

  GURL url;
  base::TimeTicks time;
  Origin origin;
  FinalStatus final_status = FINAL_STATUS_UNKNOWN;
};

PrerenderManager::PrerenderManager(
    content::BrowserContext* browser_context,
    std::unique_ptr<PrerenderManagerDelegate> delegate)
    : browser_context_(browser_context),
      delegate_(std::move(delegate)),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      prerender_history_(std::make_unique<PrerenderHistory>(kHistoryLength)),
      histograms_(std::make_unique<PrerenderHistograms>()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  last_prerender_start_time_ =
      GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

PrerenderManager::~PrerenderManager() {
  // The earlier call to KeyedService::Shutdown() should have
  // emptied these vectors already.
  DCHECK(active_prerenders_.empty());
  DCHECK(to_delete_prerenders_.empty());

  for (auto* host : prerender_process_hosts_) {
    host->RemoveObserver(this);
  }
}

void PrerenderManager::Shutdown() {
  DestroyAllContents(FINAL_STATUS_PROFILE_DESTROYED);
  on_close_web_contents_deleters_.clear();
  browser_context_ = nullptr;

  DCHECK(active_prerenders_.empty());
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    blink::mojom::PrerenderRelType rel_type,
    const content::Referrer& referrer,
    const url::Origin& initiator_origin,
    const gfx::Size& size) {
  Origin origin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  switch (rel_type) {
    case blink::mojom::PrerenderRelType::kPrerender:
      origin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
      break;
    case blink::mojom::PrerenderRelType::kNext:
      origin = ORIGIN_LINK_REL_NEXT;
      break;
  }

  SessionStorageNamespace* session_storage_namespace = nullptr;
  // Unit tests pass in a process_id == -1.
  if (process_id != -1) {
    RenderViewHost* source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host)
      return nullptr;
    WebContents* source_web_contents =
        WebContents::FromRenderViewHost(source_render_view_host);
    if (!source_web_contents)
      return nullptr;
    if (origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN &&
        source_web_contents->GetURL().host_piece() == url.host_piece()) {
      origin = ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN;
    }
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace = source_web_contents->GetController()
                                    .GetDefaultSessionStorageNamespace();
  }
  return AddPrerenderWithPreconnectFallback(origin, url, referrer,
                                            initiator_origin, gfx::Rect(size),
                                            session_storage_namespace);
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  return AddPrerenderWithPreconnectFallback(
      ORIGIN_OMNIBOX, url, content::Referrer(), base::nullopt, gfx::Rect(size),
      session_storage_namespace);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderFromNavigationPredictor(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  DCHECK(IsNoStatePrefetchEnabled());

  return AddPrerenderWithPreconnectFallback(
      ORIGIN_NAVIGATION_PREDICTOR, url, content::Referrer(), base::nullopt,
      gfx::Rect(size), session_storage_namespace);
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddIsolatedPrerender(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  DCHECK(IsNoStatePrefetchEnabled());

  // The preconnect fallback won't happen.
  return AddPrerenderWithPreconnectFallback(
      ORIGIN_ISOLATED_PRERENDER, url, content::Referrer(), base::nullopt,
      gfx::Rect(size), session_storage_namespace);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderFromExternalRequest(
    const GURL& url,
    const content::Referrer& referrer,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Rect& bounds) {
  return AddPrerenderWithPreconnectFallback(ORIGIN_EXTERNAL_REQUEST, url,
                                            referrer, base::nullopt, bounds,
                                            session_storage_namespace);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddForcedPrerenderFromExternalRequest(
    const GURL& url,
    const content::Referrer& referrer,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Rect& bounds) {
  return AddPrerenderWithPreconnectFallback(
      ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER, url, referrer, base::nullopt,
      bounds, session_storage_namespace);
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prerenders_.empty()) {
    PrerenderContents* prerender_contents =
        active_prerenders_.front()->contents();
    prerender_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(entry);

  auto it = FindIteratorForPrerenderContents(entry);
  DCHECK(it != active_prerenders_.end());
  to_delete_prerenders_.push_back(std::move(*it));
  active_prerenders_.erase(it);
  // Destroy the old WebContents relatively promptly to reduce resource usage.
  PostCleanupTask();
}

bool PrerenderManager::IsWebContentsPrerendering(
    const WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetPrerenderContents(web_contents);
}

bool PrerenderManager::HasPrerenderedUrl(
    GURL url,
    content::WebContents* web_contents) const {
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();

  for (const auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->Matches(url, session_storage_namespace))
      return true;
  }
  return false;
}

bool PrerenderManager::HasPrerenderedAndFinishedLoadingUrl(
    GURL url,
    content::WebContents* web_contents) const {
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();

  for (const auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->Matches(url, session_storage_namespace) &&
        prerender_contents->has_finished_loading()) {
      return true;
    }
  }
  return false;
}

PrerenderContents* PrerenderManager::GetPrerenderContents(
    const content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& prerender : active_prerenders_) {
    WebContents* prerender_web_contents =
        prerender->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return prerender->contents();
    }
  }

  // Also check the pending-deletion list. If the prerender is in pending
  // delete, anyone with a handle on the WebContents needs to know.
  for (const auto& prerender : to_delete_prerenders_) {
    WebContents* prerender_web_contents =
        prerender->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return prerender->contents();
    }
  }
  return nullptr;
}

PrerenderContents* PrerenderManager::GetPrerenderContentsForRoute(
    int child_id,
    int route_id) const {
  WebContents* web_contents = nullptr;
  RenderViewHost* render_view_host = RenderViewHost::FromID(child_id, route_id);
  web_contents = WebContents::FromRenderViewHost(render_view_host);
  return web_contents ? GetPrerenderContents(web_contents) : nullptr;
}

PrerenderContents* PrerenderManager::GetPrerenderContentsForProcess(
    int render_process_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->GetRenderViewHost()->GetProcess()->GetID() ==
        render_process_id) {
      return prerender_contents;
    }
  }
  return nullptr;
}

std::vector<WebContents*>
PrerenderManager::GetAllNoStatePrefetchingContentsForTesting() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<WebContents*> result;

  for (const auto& prerender : active_prerenders_) {
    WebContents* contents = prerender->contents()->prerender_contents();
    if (contents && prerender->contents()->prerender_mode() ==
                        prerender::mojom::PrerenderMode::kPrefetchOnly) {
      result.push_back(contents);
    }
  }

  return result;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(Origin origin,
                                                  const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CleanUpOldNavigations(&navigations_, base::TimeDelta::FromMilliseconds(
                                           kNavigationRecordWindowMs));
  for (auto it = navigations_.rbegin(); it != navigations_.rend(); ++it) {
    if (it->url == url)
      return true;
  }

  return false;
}

std::unique_ptr<base::DictionaryValue> PrerenderManager::CopyAsValue() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->Set("history", prerender_history_->CopyEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled",
                         delegate_->IsNetworkPredictionPreferenceEnabled());
  dict_value->SetString("disabled_note",
                        delegate_->GetReasonForDisablingPrediction());
  // If prerender is disabled via a flag this method is not even called.
  std::string enabled_note;
  dict_value->SetString("enabled_note", enabled_note);
  return dict_value;
}

void PrerenderManager::ClearData(int clear_flags) {
  DCHECK_GE(clear_flags, 0);
  DCHECK_LT(clear_flags, CLEAR_MAX);
  if (clear_flags & CLEAR_PRERENDER_CONTENTS)
    DestroyAllContents(FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  // This has to be second, since destroying prerenders can add to the history.
  if (clear_flags & CLEAR_PRERENDER_HISTORY)
    prerender_history_->Clear();
}

void PrerenderManager::RecordFinalStatus(Origin origin,
                                         FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin, final_status);
}

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  navigations_.emplace_back(url, GetCurrentTimeTicks(), ORIGIN_NONE);
  CleanUpOldNavigations(&navigations_, base::TimeDelta::FromMilliseconds(
                                           kNavigationRecordWindowMs));
}

struct PrerenderManager::PrerenderData::OrderByExpiryTime {
  bool operator()(const std::unique_ptr<PrerenderData>& a,
                  const std::unique_ptr<PrerenderData>& b) const {
    return a->expiry_time() < b->expiry_time();
  }
};

PrerenderManager::PrerenderData::PrerenderData(
    PrerenderManager* manager,
    std::unique_ptr<PrerenderContents> contents,
    base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(std::move(contents)),
      expiry_time_(expiry_time) {
  DCHECK(contents_);
}

PrerenderManager::PrerenderData::~PrerenderData() = default;

void PrerenderManager::PrerenderData::OnHandleCreated(PrerenderHandle* handle) {
  DCHECK(contents_);
  ++handle_count_;
  contents_->AddObserver(handle);
}

void PrerenderManager::PrerenderData::OnHandleNavigatedAway(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);
  if (abandon_time_.is_null())
    abandon_time_ = base::TimeTicks::Now();
  // We intentionally don't decrement the handle count here, so that the
  // prerender won't be canceled until it times out.
  manager_->SourceNavigatedAway(this);
}

void PrerenderManager::PrerenderData::OnHandleCanceled(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);

  if (--handle_count_ == 0) {
    // This will eventually remove this object from |active_prerenders_|.
    contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

std::unique_ptr<PrerenderContents>
PrerenderManager::PrerenderData::ReleaseContents() {
  return std::move(contents_);
}

void PrerenderManager::SourceNavigatedAway(PrerenderData* prerender_data) {
  // The expiry time of our prerender data will likely change because of
  // this navigation. This requires a re-sort of |active_prerenders_|.
  for (auto it = active_prerenders_.begin(); it != active_prerenders_.end();
       ++it) {
    PrerenderData* data = it->get();
    if (data == prerender_data) {
      data->set_expiry_time(std::min(data->expiry_time(),
                                     GetExpiryTimeForNavigatedAwayPrerender()));
      SortActivePrerenders();
      return;
    }
  }
}

bool PrerenderManager::IsLowEndDevice() const {
  return base::SysInfo::IsLowEndDevice();
}

bool PrerenderManager::IsPredictionEnabled(Origin origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // <link rel=prerender> and <link rel=next> origins ignore the network state
  // and the privacy
  // settings. Web developers should be able prefetch with all possible privacy
  // settings. This would avoid web devs coming up with creative ways to
  // prefetch in cases they are not allowed to do so.
  if (origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN ||
      origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
      origin == ORIGIN_LINK_REL_NEXT) {
    return true;
  }

  // TODO(crbug.com/1121970): Remove this check once we're no longer running the
  // experiment "PredictivePrefetchingAllowedOnAllConnectionTypes".
  if (delegate_->IsPredictionDisabledDueToNetwork(origin))
    return false;

  return delegate_->IsNetworkPredictionPreferenceEnabled();
}

void PrerenderManager::MaybePreconnect(Origin origin,
                                       const GURL& url_arg) const {
  delegate_->MaybePreconnect(url_arg);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderWithPreconnectFallback(
    Origin origin,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    const gfx::Rect& bounds,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Disallow prerendering on low end devices.
  if (IsLowEndDevice()) {
    SkipPrerenderContentsAndMaybePreconnect(url_arg, origin,
                                            FINAL_STATUS_LOW_END_DEVICE);
    return nullptr;
  }

  if ((origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
       origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) &&
      IsGoogleOriginURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  GURL url = url_arg;

  if (delegate_->GetCookieSettings()->ShouldBlockThirdPartyCookies()) {
    SkipPrerenderContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES);
    return nullptr;
  }

  if (!IsPredictionEnabled(origin)) {
    FinalStatus final_status =
        delegate_->IsPredictionDisabledDueToNetwork(origin)
            ? FINAL_STATUS_CELLULAR_NETWORK
            : FINAL_STATUS_PRERENDERING_DISABLED;
    SkipPrerenderContentsAndMaybePreconnect(url, origin, final_status);
    return nullptr;
  }

  if (PrerenderData* preexisting_prerender_data =
          FindPrerenderData(url, session_storage_namespace)) {
    SkipPrerenderContentsAndMaybePreconnect(url, origin,
                                            FINAL_STATUS_DUPLICATE);
    return base::WrapUnique(new PrerenderHandle(preexisting_prerender_data));
  }

  if (IsNoStatePrefetchEnabled()) {
    base::TimeDelta prefetch_age;
    GetPrefetchInformation(url, &prefetch_age, nullptr /* final_status*/,
                           nullptr /* origin */);
    if (!prefetch_age.is_zero() &&
        prefetch_age <
            base::TimeDelta::FromMinutes(net::HttpCache::kPrefetchReuseMins)) {
      SkipPrerenderContentsAndMaybePreconnect(url, origin,
                                              FINAL_STATUS_DUPLICATE);
      return nullptr;
    }
  }

  // Do not prerender if there are too many render processes, and we would
  // have to use an existing one.  We do not want prerendering to happen in
  // a shared process, so that we can always reliably lower the CPU
  // priority for prerendering.
  // In single-process mode, ShouldTryToUseExistingProcessHost() always returns
  // true, so that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prerendering in the opposite
  // case, when a new tab is added to a process used for prerendering.
  // TODO(ppi): Check whether there are usually enough render processes
  // available on Android. If not, kill an existing renderers so that we can
  // create a new one.
  if (content::RenderProcessHost::ShouldTryToUseExistingProcessHost(
          browser_context_, url) &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    SkipPrerenderContentsAndMaybePreconnect(url, origin,
                                            FINAL_STATUS_TOO_MANY_PROCESSES);
    return nullptr;
  }

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender(origin)) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    SkipPrerenderContentsAndMaybePreconnect(url, origin,
                                            FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return nullptr;
  }

  // Record the URL in the prefetch list, even when in full prerender mode, to
  // enable metrics comparisons.
  prefetches_.emplace_back(url, GetCurrentTimeTicks(), origin);

  if (GetMode() == PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT) {
    // Exit after adding the url to prefetches_, so that no prefetching occurs
    // but the page is still tracked as "would have been prefetched".
    return nullptr;
  }

  // If this is GWS and we are in the holdback, skip the prefetch. Record the
  // status as holdback, so we can analyze via UKM.
  if (origin == ORIGIN_GWS_PRERENDER &&
      base::FeatureList::IsEnabled(kGWSPrefetchHoldback)) {
    // Set the holdback status on the prefetch entry.
    SetPrefetchFinalStatusForUrl(url, FINAL_STATUS_GWS_HOLDBACK);
    SkipPrerenderContentsAndMaybePreconnect(url, origin,
                                            FINAL_STATUS_GWS_HOLDBACK);
    return nullptr;
  }

  // If this is Navigation predictor and we are in the holdback, skip the
  // prefetch. Record the status as holdback, so we can analyze via UKM.
  if (origin == ORIGIN_NAVIGATION_PREDICTOR &&
      base::FeatureList::IsEnabled(kNavigationPredictorPrefetchHoldback)) {
    // Set the holdback status on the prefetch entry.
    SetPrefetchFinalStatusForUrl(url,
                                 FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK);
    SkipPrerenderContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK);
    return nullptr;
  }

  std::unique_ptr<PrerenderContents> prerender_contents =
      CreatePrerenderContents(url, referrer, initiator_origin, origin);
  DCHECK(prerender_contents);
  PrerenderContents* prerender_contents_ptr = prerender_contents.get();
  if (IsNoStatePrefetchEnabled()) {
    prerender_contents_ptr->SetPrerenderMode(
        prerender::mojom::PrerenderMode::kPrefetchOnly);
  }
  active_prerenders_.push_back(
      std::make_unique<PrerenderData>(this, std::move(prerender_contents),
                                      GetExpiryTimeForNewPrerender(origin)));
  if (!prerender_contents_ptr->Init()) {
    DCHECK(active_prerenders_.end() ==
           FindIteratorForPrerenderContents(prerender_contents_ptr));
    return nullptr;
  }

  DCHECK(!prerender_contents_ptr->prerendering_has_started());

  std::unique_ptr<PrerenderHandle> prerender_handle =
      base::WrapUnique(new PrerenderHandle(active_prerenders_.back().get()));
  SortActivePrerenders();

  last_prerender_start_time_ = GetCurrentTimeTicks();

  gfx::Rect contents_bounds =
      bounds.IsEmpty() ? config_.default_tab_bounds : bounds;

  prerender_contents_ptr->StartPrerendering(contents_bounds,
                                            session_storage_namespace);

  DCHECK(prerender_contents_ptr->prerendering_has_started());

  StartSchedulingPeriodicCleanups();
  return prerender_handle;
}

void PrerenderManager::StartSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (repeating_timer_.IsRunning())
    return;

  repeating_timer_.Start(FROM_HERE, kPeriodicCleanupInterval, this,
                         &PrerenderManager::PeriodicCleanup);
}

void PrerenderManager::StopSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Stop();
}

void PrerenderManager::PeriodicCleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ElapsedTimer resource_timer;

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*> prerender_contents;
  prerender_contents.reserve(active_prerenders_.size());
  for (auto& prerender : active_prerenders_)
    prerender_contents.push_back(prerender->contents());

  // And now check for prerenders using too much memory.
  for (auto* contents : prerender_contents)
    contents->DestroyWhenUsingTooManyResources();

  base::ElapsedTimer cleanup_timer;

  // Perform deferred cleanup work.
  DeleteOldWebContents();
  DeleteOldEntries();
  if (active_prerenders_.empty())
    StopSchedulingPeriodicCleanups();

  DeleteToDeletePrerenders();

  CleanUpOldNavigations(&prefetches_, base::TimeDelta::FromMinutes(30));
}

void PrerenderManager::PostCleanupTask() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrerenderManager::PeriodicCleanup,
                                weak_factory_.GetWeakPtr()));
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNewPrerender(
    Origin origin) const {
  return GetCurrentTimeTicks() + config_.time_to_live;
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prerenders_.empty()) {
    auto& prerender_data = active_prerenders_.front();
    DCHECK(prerender_data);
    DCHECK(prerender_data->contents());

    if (prerender_data->expiry_time() > GetCurrentTimeTicks())
      return;
    prerender_data->contents()->Destroy(FINAL_STATUS_TIMED_OUT);
  }
}

void PrerenderManager::DeleteToDeletePrerenders() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delete the items one by one (after removing from the vector) as deleting
  // the WebContents may trigger a call to GetPrerenderContents(), which
  // iterates over |to_delete_prerenders_|.
  while (!to_delete_prerenders_.empty()) {
    std::unique_ptr<PrerenderData> prerender_data =
        std::move(to_delete_prerenders_.back());
    to_delete_prerenders_.pop_back();
  }
}

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return tick_clock_->NowTicks();
}

void PrerenderManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void PrerenderManager::AddObserver(
    std::unique_ptr<PrerenderManagerObserver> observer) {
  observers_.push_back(std::move(observer));
}

std::unique_ptr<PrerenderContents> PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const content::Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::WrapUnique(prerender_contents_factory_->CreatePrerenderContents(
      delegate_->GetPrerenderContentsDelegate(), this, browser_context_, url,
      referrer, initiator_origin, origin));
}

void PrerenderManager::SortActivePrerenders() {
  std::sort(active_prerenders_.begin(), active_prerenders_.end(),
            PrerenderData::OrderByExpiryTime());
}

PrerenderManager::PrerenderData* PrerenderManager::FindPrerenderData(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace) {
  for (const auto& prerender : active_prerenders_) {
    PrerenderContents* contents = prerender->contents();
    if (contents->Matches(url, session_storage_namespace))
      return prerender.get();
  }
  return nullptr;
}

PrerenderManager::PrerenderDataVector::iterator
PrerenderManager::FindIteratorForPrerenderContents(
    PrerenderContents* prerender_contents) {
  for (auto it = active_prerenders_.begin(); it != active_prerenders_.end();
       ++it) {
    if ((*it)->contents() == prerender_contents)
      return it;
  }
  return active_prerenders_.end();
}

bool PrerenderManager::DoesRateLimitAllowPrerender(Origin origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Allow navigation predictor to manage its own rate limit.
  if (origin == ORIGIN_NAVIGATION_PREDICTOR)
    return true;
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >=
         base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::DeleteOldWebContents() {
  old_web_contents_list_.clear();
}

bool PrerenderManager::GetPrefetchInformation(const GURL& url,
                                              base::TimeDelta* prefetch_age,
                                              FinalStatus* final_status,
                                              Origin* origin) {
  CleanUpOldNavigations(&prefetches_, base::TimeDelta::FromMinutes(30));

  if (prefetch_age)
    *prefetch_age = base::TimeDelta();
  if (final_status)
    *final_status = FINAL_STATUS_MAX;
  if (origin)
    *origin = ORIGIN_NONE;

  for (auto it = prefetches_.crbegin(); it != prefetches_.crend(); ++it) {
    if (it->url == url) {
      if (prefetch_age)
        *prefetch_age = GetCurrentTimeTicks() - it->time;
      if (final_status)
        *final_status = it->final_status;
      if (origin)
        *origin = it->origin;
      return true;
    }
  }
  return false;
}

void PrerenderManager::SetPrefetchFinalStatusForUrl(const GURL& url,
                                                    FinalStatus final_status) {
  for (auto it = prefetches_.rbegin(); it != prefetches_.rend(); ++it) {
    if (it->url == url) {
      it->final_status = final_status;
      break;
    }
  }
}

bool PrerenderManager::HasRecentlyPrefetchedUrlForTesting(const GURL& url) {
  return std::any_of(prefetches_.cbegin(), prefetches_.cend(),
                     [url](const NavigationRecord& r) {
                       return r.url == url &&
                              r.final_status ==
                                  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED;
                     });
}

void PrerenderManager::OnPrefetchUsed(const GURL& url) {
  // Loading a prefetched URL resets the revalidation bypass. Remove all
  // matching urls from the prefetch list for more accurate metrics.
  base::EraseIf(prefetches_,
                [url](const NavigationRecord& r) { return r.url == url; });
}

void PrerenderManager::CleanUpOldNavigations(
    std::vector<NavigationRecord>* navigations,
    base::TimeDelta max_age) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Cutoff. Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() - max_age;
  auto it = navigations->begin();
  for (; it != navigations->end(); ++it) {
    if (it->time > cutoff)
      break;
  }
  navigations->erase(navigations->begin(), it);
}

void PrerenderManager::ScheduleDeleteOldWebContents(
    std::unique_ptr<WebContents> tab,
    OnCloseWebContentsDeleter* deleter) {
  old_web_contents_list_.push_back(std::move(tab));
  PostCleanupTask();

  if (!deleter)
    return;

  for (auto it = on_close_web_contents_deleters_.begin();
       it != on_close_web_contents_deleters_.end(); ++it) {
    if (it->get() == deleter) {
      on_close_web_contents_deleters_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(), contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

std::unique_ptr<base::ListValue> PrerenderManager::GetActivePrerendersAsValue()
    const {
  auto list_value = std::make_unique<base::ListValue>();
  for (const auto& prerender : active_prerenders_) {
    auto prerender_value = prerender->contents()->GetAsValue();
    if (prerender_value)
      list_value->Append(std::move(prerender_value));
  }
  return list_value;
}

void PrerenderManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldWebContents();
  while (!active_prerenders_.empty()) {
    PrerenderContents* contents = active_prerenders_.front()->contents();
    contents->Destroy(final_status);
  }
  DeleteToDeletePrerenders();
}

void PrerenderManager::SkipPrerenderContentsAndMaybePreconnect(
    const GURL& url,
    Origin origin,
    FinalStatus final_status) const {
  PrerenderHistory::Entry entry(url, final_status, origin, base::Time::Now());
  prerender_history_->AddEntry(entry);
  histograms_->RecordFinalStatus(origin, final_status);

  if (origin == ORIGIN_ISOLATED_PRERENDER) {
    // Isolated Prerenders should not preconnect since that can't be done in a
    // fully isolated way.
    return;
  }

  if (final_status == FINAL_STATUS_LOW_END_DEVICE ||
      final_status == FINAL_STATUS_CELLULAR_NETWORK ||
      final_status == FINAL_STATUS_DUPLICATE ||
      final_status == FINAL_STATUS_TOO_MANY_PROCESSES) {
    MaybePreconnect(origin, url);
  }

  static_assert(
      FINAL_STATUS_MAX == FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK + 1,
      "Consider whether a failed prerender should fallback to preconnect");
}

void PrerenderManager::RecordNetworkBytesConsumed(Origin origin,
                                                  int64_t prerender_bytes) {
  if (!IsNoStatePrefetchEnabled())
    return;
  int64_t recent_browser_context_bytes =
      browser_context_network_bytes_ -
      last_recorded_browser_context_network_bytes_;
  last_recorded_browser_context_network_bytes_ = browser_context_network_bytes_;
  DCHECK_GE(recent_browser_context_bytes, 0);
  histograms_->RecordNetworkBytesConsumed(origin, prerender_bytes,
                                          recent_browser_context_bytes);
}

void PrerenderManager::AddPrerenderProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool inserted = prerender_process_hosts_.insert(process_host).second;
  DCHECK(inserted);
  process_host->AddObserver(this);
}

bool PrerenderManager::MayReuseProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Isolate prerender processes to make the resource monitoring check more
  // accurate.
  return !base::Contains(prerender_process_hosts_, process_host);
}

void PrerenderManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t erased = prerender_process_hosts_.erase(host);
  DCHECK_EQ(1u, erased);
}

base::WeakPtr<PrerenderManager> PrerenderManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderManager::ClearPrefetchInformationForTesting() {
  prefetches_.clear();
}

void PrerenderManager::SetPrerenderContentsFactoryForTest(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prerender_contents_factory_.reset(prerender_contents_factory);
}

}  // namespace prerender
