// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"

#include <stddef.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_history.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_utils.h"
#include "components/no_state_prefetch/browser/prerender_histograms.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "ui/gfx/geometry/rect.h"

using content::BrowserThread;
using content::PreloadingAttempt;
using content::PreloadingEligibility;
using content::PreloadingFailureReason;
using content::PreloadingHoldbackStatus;
using content::PreloadingTriggeringOutcome;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

// Time interval at which periodic cleanups are performed.
constexpr base::TimeDelta kPeriodicCleanupInterval = base::Milliseconds(1000);

// Time interval after which OnCloseWebContentsDeleter will schedule a
// WebContents for deletion.
constexpr base::TimeDelta kDeleteWithExtremePrejudice = base::Seconds(3);

// Length of NoStatePrefetch history, for display in chrome://net-internals.
constexpr int kHistoryLength = 100;

// Helper methods to set PrelodingAttempt fields.
PreloadingFailureReason ToPreloadingFailureReason(FinalStatus status) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonContentEnd));
}

void SetPreloadingTriggeringOutcome(PreloadingAttempt* attempt,
                                    PreloadingTriggeringOutcome outcome) {
  if (!attempt)
    return;

  attempt->SetTriggeringOutcome(outcome);
}

void SetPreloadingEligibility(PreloadingAttempt* attempt,
                              PreloadingEligibility eligibility) {
  if (!attempt)
    return;

  attempt->SetEligibility(eligibility);
}

}  // namespace

class NoStatePrefetchManager::OnCloseWebContentsDeleter final
    : public content::WebContentsDelegate {
 public:
  OnCloseWebContentsDeleter(NoStatePrefetchManager* manager,
                            std::unique_ptr<WebContents> tab)
      : manager_(manager), tab_(std::move(tab)) {
    tab_->SetOwnerLocationForDebug(FROM_HERE);
    tab_->SetDelegate(this);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &OnCloseWebContentsDeleter::ScheduleWebContentsForDeletion,
            weak_factory_.GetWeakPtr()),
        kDeleteWithExtremePrejudice);
  }

  OnCloseWebContentsDeleter(const OnCloseWebContentsDeleter&) = delete;
  OnCloseWebContentsDeleter& operator=(const OnCloseWebContentsDeleter&) =
      delete;

  void CloseContents(WebContents* source) override {
    DCHECK_EQ(tab_.get(), source);
    ScheduleWebContentsForDeletion();
  }

 private:
  void ScheduleWebContentsForDeletion() {
    tab_->SetDelegate(nullptr);
    tab_->SetOwnerLocationForDebug(std::nullopt);
    manager_->ScheduleDeleteOldWebContents(std::move(tab_), this);
    // |this| is deleted at this point.
  }

  const raw_ptr<NoStatePrefetchManager> manager_;
  std::unique_ptr<WebContents> tab_;
  base::WeakPtrFactory<OnCloseWebContentsDeleter> weak_factory_{this};
};

NoStatePrefetchManagerObserver::~NoStatePrefetchManagerObserver() = default;

struct NoStatePrefetchManager::NavigationRecord {
  NavigationRecord(const GURL& url, base::TimeTicks time, Origin origin)
      : url(url), time(time), origin(origin) {}

  GURL url;
  base::TimeTicks time;
  Origin origin;
  FinalStatus final_status = FINAL_STATUS_UNKNOWN;
};

NoStatePrefetchManager::NoStatePrefetchManager(
    content::BrowserContext* browser_context,
    std::unique_ptr<NoStatePrefetchManagerDelegate> delegate)
    : browser_context_(browser_context),
      delegate_(std::move(delegate)),
      no_state_prefetch_contents_factory_(
          NoStatePrefetchContents::CreateFactory()),
      prefetch_history_(
          std::make_unique<NoStatePrefetchHistory>(kHistoryLength)),
      histograms_(std::make_unique<PrerenderHistograms>()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  last_prefetch_start_time_ =
      GetCurrentTimeTicks() - base::Milliseconds(kMinTimeBetweenPrefetchesMs);
}

NoStatePrefetchManager::~NoStatePrefetchManager() {
  // The earlier call to KeyedService::Shutdown() should have
  // emptied these vectors already.
  DCHECK(active_prefetches_.empty());
  DCHECK(to_delete_prefetches_.empty());

  for (content::RenderProcessHost* host : prerender_process_hosts_) {
    host->RemoveObserver(this);
  }
}

void NoStatePrefetchManager::Shutdown() {
  DestroyAllContents(FINAL_STATUS_PROFILE_DESTROYED);
  on_close_web_contents_deleters_.clear();
  browser_context_ = nullptr;

  DCHECK(active_prefetches_.empty());
}

std::unique_ptr<NoStatePrefetchHandle>
NoStatePrefetchManager::StartPrefetchingFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    blink::mojom::PrerenderTriggerType trigger_type,
    const content::Referrer& referrer,
    const url::Origin& initiator_origin,
    const gfx::Size& size) {
  Origin origin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  switch (trigger_type) {
    case blink::mojom::PrerenderTriggerType::kLinkRelPrerender:
      origin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
      break;
    case blink::mojom::PrerenderTriggerType::kLinkRelNext:
      origin = ORIGIN_LINK_REL_NEXT;
      break;
  }

  SessionStorageNamespace* session_storage_namespace = nullptr;
  PreloadingAttempt* attempt = nullptr;

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
        source_web_contents->GetVisibleURL().host_piece() == url.host_piece()) {
      origin = ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN;
    }
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace = source_web_contents->GetController()
                                    .GetDefaultSessionStorageNamespace();
    // Create new PreloadingPrediction class and pass all the fields.
    content::PreloadingURLMatchCallback same_url_matcher =
        content::PreloadingData::GetSameURLMatcher(url);

    auto* preloading_data =
        content::PreloadingData::GetOrCreateForWebContents(source_web_contents);
    // In case of link-rel, the confidence is set as 100 as the URL
    // was not predicted and confidence in this case is not defined.
    int confidence = 100;

    // Create PreloadingPrediction and PreloadingAttempt for NoStatePrefetch.
    ukm::SourceId triggered_primary_page_source_id =
        source_web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    preloading_data->AddPreloadingPrediction(
        content::preloading_predictor::kLinkRel, confidence, same_url_matcher,
        triggered_primary_page_source_id);
    attempt = preloading_data->AddPreloadingAttempt(
        content::preloading_predictor::kLinkRel,
        content::PreloadingType::kNoStatePrefetch, same_url_matcher,
        /*planned_max_preloading_type=*/std::nullopt,
        triggered_primary_page_source_id);
  }
  return StartPrefetchingWithPreconnectFallback(
      origin, url, referrer, initiator_origin, gfx::Rect(size),
      session_storage_namespace, attempt ? attempt->GetWeakPtr() : nullptr);
}

std::unique_ptr<NoStatePrefetchHandle>
NoStatePrefetchManager::AddSameOriginSpeculation(
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size,
    const url::Origin& initiator_origin) {
  // The preconnect fallback won't happen.
  return StartPrefetchingWithPreconnectFallback(
      ORIGIN_SAME_ORIGIN_SPECULATION, url, content::Referrer(),
      initiator_origin, gfx::Rect(size), session_storage_namespace);
}

void NoStatePrefetchManager::CancelAllPrerenders() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prefetches_.empty()) {
    NoStatePrefetchContents* no_state_prefetch_contents =
        active_prefetches_.front()->contents();
    no_state_prefetch_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

void NoStatePrefetchManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldWebContents();
  while (!active_prefetches_.empty()) {
    NoStatePrefetchContents* contents = active_prefetches_.front()->contents();
    contents->Destroy(final_status);
  }
  DeleteToDeletePrerenders();
}

void NoStatePrefetchManager::MoveEntryToPendingDelete(
    NoStatePrefetchContents* entry,
    FinalStatus final_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(entry);

  auto it = FindIteratorForNoStatePrefetchContents(entry);
  CHECK(it != active_prefetches_.end(), base::NotFatalUntil::M130);
  to_delete_prefetches_.push_back(std::move(*it));
  active_prefetches_.erase(it);
  // Destroy the old WebContents relatively promptly to reduce resource usage.
  PostCleanupTask();
}

bool NoStatePrefetchManager::IsWebContentsPrefetching(
    const WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetNoStatePrefetchContents(web_contents);
}

NoStatePrefetchContents* NoStatePrefetchManager::GetNoStatePrefetchContents(
    const content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& prefetch : active_prefetches_) {
    WebContents* prefetch_web_contents =
        prefetch->contents()->no_state_prefetch_contents();
    if (prefetch_web_contents == web_contents) {
      return prefetch->contents();
    }
  }

  // Also check the pending-deletion list. If the prefetch is in pending delete,
  // anyone with a handle on the WebContents needs to know.
  for (const auto& prefetch : to_delete_prefetches_) {
    WebContents* prefetch_web_contents =
        prefetch->contents()->no_state_prefetch_contents();
    if (prefetch_web_contents == web_contents) {
      return prefetch->contents();
    }
  }
  return nullptr;
}

NoStatePrefetchContents*
NoStatePrefetchManager::GetNoStatePrefetchContentsForRoute(int child_id,
                                                           int route_id) const {
  WebContents* web_contents = nullptr;
  RenderViewHost* render_view_host = RenderViewHost::FromID(child_id, route_id);
  web_contents = WebContents::FromRenderViewHost(render_view_host);
  return web_contents ? GetNoStatePrefetchContents(web_contents) : nullptr;
}

std::vector<WebContents*>
NoStatePrefetchManager::GetAllNoStatePrefetchingContentsForTesting() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<WebContents*> result;

  for (const auto& prefetch : active_prefetches_) {
    WebContents* contents = prefetch->contents()->no_state_prefetch_contents();
    if (contents)
      result.push_back(contents);
  }

  return result;
}

bool NoStatePrefetchManager::HasRecentlyBeenNavigatedTo(Origin origin,
                                                        const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CleanUpOldNavigations(&navigations_,
                        base::Milliseconds(kNavigationRecordWindowMs));
  for (const NavigationRecord& navigation : base::Reversed(navigations_)) {
    if (navigation.url == url)
      return true;
  }

  return false;
}
base::Value::Dict NoStatePrefetchManager::CopyAsDict() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value::Dict dict_value;
  dict_value.Set("history", prefetch_history_->CopyEntriesAsValue());
  dict_value.Set("active", GetActivePrerenders());
  dict_value.Set("enabled", delegate_->IsNetworkPredictionPreferenceEnabled());
  dict_value.Set("disabled_note", delegate_->GetReasonForDisablingPrediction());
  // If prerender is disabled via a flag this method is not even called.
  dict_value.Set("enabled_note", std::string());
  return dict_value;
}

void NoStatePrefetchManager::ClearData(int clear_flags) {
  DCHECK_GE(clear_flags, 0);
  DCHECK_LT(clear_flags, CLEAR_MAX);
  if (clear_flags & CLEAR_PRERENDER_CONTENTS)
    DestroyAllContents(FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  // This has to be second, since destroying prerenders can add to the history.
  if (clear_flags & CLEAR_PRERENDER_HISTORY)
    prefetch_history_->Clear();
}

void NoStatePrefetchManager::RecordFinalStatus(Origin origin,
                                               FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin, final_status);
}

void NoStatePrefetchManager::RecordNavigation(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  navigations_.emplace_back(url, GetCurrentTimeTicks(), ORIGIN_NONE);
  CleanUpOldNavigations(&navigations_,
                        base::Milliseconds(kNavigationRecordWindowMs));
}

struct NoStatePrefetchManager::NoStatePrefetchData::OrderByExpiryTime {
  bool operator()(const std::unique_ptr<NoStatePrefetchData>& a,
                  const std::unique_ptr<NoStatePrefetchData>& b) const {
    return a->expiry_time() < b->expiry_time();
  }
};

NoStatePrefetchManager::NoStatePrefetchData::NoStatePrefetchData(
    NoStatePrefetchManager* manager,
    std::unique_ptr<NoStatePrefetchContents> contents,
    base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(std::move(contents)),
      expiry_time_(expiry_time) {
  DCHECK(contents_);
}

NoStatePrefetchManager::NoStatePrefetchData::~NoStatePrefetchData() = default;

void NoStatePrefetchManager::NoStatePrefetchData::OnHandleCreated(
    NoStatePrefetchHandle* handle) {
  DCHECK(contents_);
  ++handle_count_;
  contents_->AddObserver(handle);
}

void NoStatePrefetchManager::NoStatePrefetchData::OnHandleNavigatedAway(
    NoStatePrefetchHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);
  // We intentionally don't decrement the handle count here, so that the
  // prefetch won't be canceled until it times out.
  manager_->SourceNavigatedAway(this);
}

void NoStatePrefetchManager::NoStatePrefetchData::OnHandleCanceled(
    NoStatePrefetchHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);

  if (--handle_count_ == 0) {
    // This will eventually remove this object from |active_prefetches_|.
    contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

std::unique_ptr<NoStatePrefetchContents>
NoStatePrefetchManager::NoStatePrefetchData::ReleaseContents() {
  return std::move(contents_);
}

void NoStatePrefetchManager::SourceNavigatedAway(
    NoStatePrefetchData* prefetch_data) {
  // The expiry time of our prefetch data will likely change because of
  // this navigation. This requires a re-sort of |active_prefetches_|.
  for (auto it = active_prefetches_.begin(); it != active_prefetches_.end();
       ++it) {
    NoStatePrefetchData* data = it->get();
    if (data == prefetch_data) {
      data->set_expiry_time(std::min(data->expiry_time(),
                                     GetExpiryTimeForNavigatedAwayPrerender()));
      SortActivePrefetches();
      return;
    }
  }
}

bool NoStatePrefetchManager::IsLowEndDevice() const {
  return base::SysInfo::IsLowEndDevice();
}

bool NoStatePrefetchManager::IsPredictionEnabled(Origin origin) {
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

  return delegate_->IsNetworkPredictionPreferenceEnabled();
}

void NoStatePrefetchManager::MaybePreconnect(Origin origin,
                                             const GURL& url_arg) const {
  delegate_->MaybePreconnect(url_arg);
}

std::unique_ptr<NoStatePrefetchHandle>
NoStatePrefetchManager::StartPrefetchingWithPreconnectFallback(
    Origin origin,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    const gfx::Rect& bounds,
    SessionStorageNamespace* session_storage_namespace,
    base::WeakPtr<content::PreloadingAttempt> attempt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(switches::kSingleProcess)) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(url_arg, origin,
                                                  FINAL_STATUS_SINGLE_PROCESS);
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kSingleProcess);
    return nullptr;
  }

  // Disallow NSPing link-rel:next URLs.
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1158209.
  if (origin == ORIGIN_LINK_REL_NEXT) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url_arg, origin, FINAL_STATUS_LINK_REL_NEXT_NOT_ALLOWED);
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kLinkRelNext);
    return nullptr;
  }

  // Disallow prerendering on low end devices.
  if (IsLowEndDevice()) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(url_arg, origin,
                                                  FINAL_STATUS_LOW_END_DEVICE);
    SetPreloadingEligibility(attempt.get(), PreloadingEligibility::kLowMemory);
    return nullptr;
  }

  if ((origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
       origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) &&
      IsGoogleOriginURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  GURL url = url_arg;

  if (delegate_->GetCookieSettings()->ShouldBlockThirdPartyCookies()) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES);
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kThirdPartyCookies);
    return nullptr;
  }

  if (!IsPredictionEnabled(origin)) {
    FinalStatus final_status = FINAL_STATUS_PRERENDERING_DISABLED;
    SkipNoStatePrefetchContentsAndMaybePreconnect(url, origin, final_status);
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kPreloadingDisabled);
    return nullptr;
  }

  // Check if enough time has passed since the last prefetch.
  if (!DoesRateLimitAllowPrefetch(origin)) {
    // Cancel the prefetch. We could add it to the pending prefetch list but
    // this doesn't make sense as the next prefetch request will be triggered
    // by a navigation and is unlikely to be the same site.
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    SetPreloadingEligibility(
        attempt.get(),
        PreloadingEligibility::kPreloadingInvokedWithinTimelimit);
    return nullptr;
  }

  // If this is GWS and we are in the holdback, skip the prefetch. Record the
  // status as holdback, so we can analyze via UKM.
  if (origin == ORIGIN_GWS_PRERENDER &&
      base::FeatureList::IsEnabled(kGWSPrefetchHoldback)) {
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kPreloadingDisabled);
    // Set the holdback status on the prefetch entry.
    SetPrefetchFinalStatusForUrl(url, FINAL_STATUS_GWS_HOLDBACK);
    SkipNoStatePrefetchContentsAndMaybePreconnect(url, origin,
                                                  FINAL_STATUS_GWS_HOLDBACK);
    return nullptr;
  }

  // If this is Navigation predictor and we are in the holdback, skip the
  // prefetch. Record the status as holdback, so we can analyze via UKM.
  if (origin == ORIGIN_NAVIGATION_PREDICTOR &&
      base::FeatureList::IsEnabled(kNavigationPredictorPrefetchHoldback)) {
    // Set the holdback status on the prefetch entry.
    SetPreloadingEligibility(attempt.get(),
                             PreloadingEligibility::kPreloadingDisabled);
    SetPrefetchFinalStatusForUrl(url,
                                 FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK);
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_NAVIGATION_PREDICTOR_HOLDBACK);
    return nullptr;
  }

  // NoStatePrefetch is now eligible to be triggered.
  SetPreloadingEligibility(attempt.get(), PreloadingEligibility::kEligible);

  // In addition to the globally-controlled preloading config, check for the
  // feature-specific holdback. We disable the feature if the user is in either
  // of those holdbacks.
  if (base::FeatureList::IsEnabled(features::kNoStatePrefetchHoldback)) {
    if (attempt) {
      attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kHoldback);
    }
  }
  if (attempt && attempt->ShouldHoldback()) {
    SetPrefetchFinalStatusForUrl(url, FINAL_STATUS_PREFETCH_HOLDBACK);
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_PREFETCH_HOLDBACK);
    return nullptr;
  }

  if (NoStatePrefetchData* preexisting_prefetch_data =
          FindNoStatePrefetchData(url, session_storage_namespace)) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(url, origin,
                                                  FINAL_STATUS_DUPLICATE);
    SetPreloadingTriggeringOutcome(attempt.get(),
                                   PreloadingTriggeringOutcome::kDuplicate);
    return base::WrapUnique(
        new NoStatePrefetchHandle(preexisting_prefetch_data));
  }

  base::TimeDelta prefetch_age;
  GetPrefetchInformation(url, &prefetch_age, nullptr /* final_status*/,
                         nullptr /* origin */);
  if (!prefetch_age.is_zero() &&
      prefetch_age < base::Minutes(net::HttpCache::kPrefetchReuseMins)) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(url, origin,
                                                  FINAL_STATUS_DUPLICATE);
    SetPreloadingTriggeringOutcome(attempt.get(),
                                   PreloadingTriggeringOutcome::kDuplicate);
    return nullptr;
  }

  // Do not prefetch if there are too many render processes, and we would have
  // to use an existing one.  We do not want prefetching to happen in a shared
  // process, so that we can always reliably lower the CPU priority for
  // prefetching.
  // In single-process mode, IsProcessLimitReached() always returns true, so
  // that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prefetching in the opposite case,
  // when a new tab is added to a process used for prefetching.
  // TODO(ppi): Check whether there are usually enough render processes
  // available on Android. If not, kill an existing renderers so that we can
  // create a new one.
  if (content::RenderProcessHost::IsProcessLimitReached() &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    SkipNoStatePrefetchContentsAndMaybePreconnect(
        url, origin, FINAL_STATUS_TOO_MANY_PROCESSES);
    // Since it is possible that the NSP enabled group uses more processes, we
    // don't see a similar behavior in our holdback and enabled group for
    // counterfactual logging. Setting this as a failure reason to capture this
    // behavior consistently.
    if (attempt) {
      attempt->SetFailureReason(
          ToPreloadingFailureReason(FINAL_STATUS_TOO_MANY_PROCESSES));
    }
    return nullptr;
  }

  // Record the URL in the prefetch list, even when in full prerender mode, to
  // enable metrics comparisons.
  prefetches_.emplace_back(url, GetCurrentTimeTicks(), origin);

  std::unique_ptr<NoStatePrefetchContents> no_state_prefetch_contents =
      CreateNoStatePrefetchContents(url, referrer, initiator_origin, origin);
  DCHECK(no_state_prefetch_contents);
  NoStatePrefetchContents* no_state_prefetch_contents_ptr =
      no_state_prefetch_contents.get();
  active_prefetches_.push_back(std::make_unique<NoStatePrefetchData>(
      this, std::move(no_state_prefetch_contents),
      GetExpiryTimeForNewPrerender(origin)));
  if (!no_state_prefetch_contents_ptr->Init()) {
    DCHECK(active_prefetches_.end() == FindIteratorForNoStatePrefetchContents(
                                           no_state_prefetch_contents_ptr));
    return nullptr;
  }

  DCHECK(!no_state_prefetch_contents_ptr->prefetching_has_started());

  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle =
      base::WrapUnique(
          new NoStatePrefetchHandle(active_prefetches_.back().get()));
  SortActivePrefetches();

  last_prefetch_start_time_ = GetCurrentTimeTicks();

  gfx::Rect contents_bounds =
      bounds.IsEmpty() ? config_.default_tab_bounds : bounds;

  no_state_prefetch_contents_ptr->StartPrerendering(
      contents_bounds, session_storage_namespace, attempt);

  DCHECK(no_state_prefetch_contents_ptr->prefetching_has_started());

  StartSchedulingPeriodicCleanups();
  return no_state_prefetch_handle;
}

void NoStatePrefetchManager::StartSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (repeating_timer_.IsRunning())
    return;

  repeating_timer_.Start(FROM_HERE, kPeriodicCleanupInterval, this,
                         &NoStatePrefetchManager::PeriodicCleanup);
}

void NoStatePrefetchManager::StopSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Stop();
}

void NoStatePrefetchManager::PeriodicCleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ElapsedTimer resource_timer;

  // Grab a copy of the current NoStatePrefetchContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<NoStatePrefetchContents*> prefetch_contents;
  prefetch_contents.reserve(active_prefetches_.size());
  for (auto& prefetch : active_prefetches_)
    prefetch_contents.push_back(prefetch->contents());

  // And now check for prerenders using too much memory.
  for (auto* contents : prefetch_contents)
    contents->DestroyWhenUsingTooManyResources();

  base::ElapsedTimer cleanup_timer;

  // Perform deferred cleanup work.
  DeleteOldWebContents();
  DeleteOldEntries();
  if (active_prefetches_.empty())
    StopSchedulingPeriodicCleanups();

  DeleteToDeletePrerenders();

  CleanUpOldNavigations(&prefetches_, base::Minutes(30));
}

void NoStatePrefetchManager::PostCleanupTask() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NoStatePrefetchManager::PeriodicCleanup,
                                weak_factory_.GetWeakPtr()));
}

base::TimeTicks NoStatePrefetchManager::GetExpiryTimeForNewPrerender(
    Origin origin) const {
  return GetCurrentTimeTicks() + config_.time_to_live;
}

base::TimeTicks NoStatePrefetchManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void NoStatePrefetchManager::DeleteOldEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prefetches_.empty()) {
    auto& prefetch_data = active_prefetches_.front();
    DCHECK(prefetch_data);
    DCHECK(prefetch_data->contents());

    if (prefetch_data->expiry_time() > GetCurrentTimeTicks())
      return;
    prefetch_data->contents()->Destroy(FINAL_STATUS_TIMED_OUT);
  }
}

void NoStatePrefetchManager::DeleteToDeletePrerenders() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delete the items one by one (after removing from the vector) as deleting
  // the WebContents may trigger a call to GetNoStatePrefetchContents(), which
  // iterates over |to_delete_prefetches_|.
  while (!to_delete_prefetches_.empty()) {
    std::unique_ptr<NoStatePrefetchData> prefetch_data =
        std::move(to_delete_prefetches_.back());
    to_delete_prefetches_.pop_back();
  }
}

base::Time NoStatePrefetchManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks NoStatePrefetchManager::GetCurrentTimeTicks() const {
  return tick_clock_->NowTicks();
}

void NoStatePrefetchManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void NoStatePrefetchManager::AddObserver(
    std::unique_ptr<NoStatePrefetchManagerObserver> observer) {
  observers_.push_back(std::move(observer));
}

std::unique_ptr<NoStatePrefetchContents>
NoStatePrefetchManager::CreateNoStatePrefetchContents(
    const GURL& url,
    const content::Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::WrapUnique(
      no_state_prefetch_contents_factory_->CreateNoStatePrefetchContents(
          delegate_->GetNoStatePrefetchContentsDelegate(), this,
          browser_context_, url, referrer, initiator_origin, origin));
}

void NoStatePrefetchManager::SortActivePrefetches() {
  std::sort(active_prefetches_.begin(), active_prefetches_.end(),
            NoStatePrefetchData::OrderByExpiryTime());
}

NoStatePrefetchManager::NoStatePrefetchData*
NoStatePrefetchManager::FindNoStatePrefetchData(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace) {
  for (const auto& prefetch : active_prefetches_) {
    NoStatePrefetchContents* contents = prefetch->contents();
    if (contents->Matches(url, session_storage_namespace))
      return prefetch.get();
  }
  return nullptr;
}

NoStatePrefetchManager::NoStatePrefetchDataVector::iterator
NoStatePrefetchManager::FindIteratorForNoStatePrefetchContents(
    NoStatePrefetchContents* no_state_prefetch_contents) {
  for (auto it = active_prefetches_.begin(); it != active_prefetches_.end();
       ++it) {
    if ((*it)->contents() == no_state_prefetch_contents)
      return it;
  }
  return active_prefetches_.end();
}

bool NoStatePrefetchManager::DoesRateLimitAllowPrefetch(Origin origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Allow navigation predictor to manage its own rate limit.
  if (origin == ORIGIN_NAVIGATION_PREDICTOR)
    return true;
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prefetch_start_time_;
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >= base::Milliseconds(kMinTimeBetweenPrefetchesMs);
}

void NoStatePrefetchManager::DeleteOldWebContents() {
  old_web_contents_list_.clear();
}

bool NoStatePrefetchManager::GetPrefetchInformation(
    const GURL& url,
    base::TimeDelta* prefetch_age,
    FinalStatus* final_status,
    Origin* origin) {
  CleanUpOldNavigations(&prefetches_, base::Minutes(30));

  if (prefetch_age)
    *prefetch_age = base::TimeDelta();
  if (final_status)
    *final_status = FINAL_STATUS_MAX;
  if (origin)
    *origin = ORIGIN_NONE;

  for (const NavigationRecord& prefetch : base::Reversed(prefetches_)) {
    if (prefetch.url == url) {
      if (prefetch_age)
        *prefetch_age = GetCurrentTimeTicks() - prefetch.time;
      if (final_status)
        *final_status = prefetch.final_status;
      if (origin)
        *origin = prefetch.origin;
      return true;
    }
  }
  return false;
}

void NoStatePrefetchManager::SetPrefetchFinalStatusForUrl(
    const GURL& url,
    FinalStatus final_status) {
  for (NavigationRecord& prefetch : base::Reversed(prefetches_)) {
    if (prefetch.url == url) {
      prefetch.final_status = final_status;
      break;
    }
  }
}

void NoStatePrefetchManager::OnPrefetchUsed(const GURL& url) {
  // Loading a prefetched URL resets the revalidation bypass. Remove all
  // matching urls from the prefetch list for more accurate metrics.
  std::erase_if(prefetches_,
                [url](const NavigationRecord& r) { return r.url == url; });
}

void NoStatePrefetchManager::CleanUpOldNavigations(
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

void NoStatePrefetchManager::ScheduleDeleteOldWebContents(
    std::unique_ptr<WebContents> tab,
    OnCloseWebContentsDeleter* deleter) {
  if (tab) {
    tab->SetOwnerLocationForDebug(FROM_HERE);
  }
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
  NOTREACHED_IN_MIGRATION();
}

void NoStatePrefetchManager::AddToHistory(NoStatePrefetchContents* contents) {
  NoStatePrefetchHistory::Entry entry(contents->prefetch_url(),
                                      contents->final_status(),
                                      contents->origin(), base::Time::Now());
  prefetch_history_->AddEntry(entry);
}

base::Value::List NoStatePrefetchManager::GetActivePrerenders() const {
  base::Value::List list;
  for (const auto& prefetch : active_prefetches_) {
    if (std::optional<base::Value::Dict> prefetch_value =
            prefetch->contents()->GetAsDict()) {
      list.Append(std::move(*prefetch_value));
    }
  }
  return list;
}

void NoStatePrefetchManager::SkipNoStatePrefetchContentsAndMaybePreconnect(
    const GURL& url,
    Origin origin,
    FinalStatus final_status) const {
  NoStatePrefetchHistory::Entry entry(url, final_status, origin,
                                      base::Time::Now());
  prefetch_history_->AddEntry(entry);
  histograms_->RecordFinalStatus(origin, final_status);

  if (origin == ORIGIN_SAME_ORIGIN_SPECULATION) {
    // Prefetch Proxy should not preconnect since that can't be done in a fully
    // isolated way. Same origin speculation should already have an open
    // connection.
    return;
  }

  if (origin == ORIGIN_LINK_REL_NEXT)
    return;

  if (final_status == FINAL_STATUS_LOW_END_DEVICE ||
      final_status == FINAL_STATUS_DUPLICATE ||
      final_status == FINAL_STATUS_TOO_MANY_PROCESSES) {
    MaybePreconnect(origin, url);
  }

  static_assert(
      FINAL_STATUS_MAX == FINAL_STATUS_PREFETCH_HOLDBACK + 1,
      "Consider whether a failed prefetch should fallback to preconnect");
}

void NoStatePrefetchManager::AddPrerenderProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool inserted = prerender_process_hosts_.insert(process_host).second;
  DCHECK(inserted);
  process_host->AddObserver(this);
}

bool NoStatePrefetchManager::MayReuseProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Isolate prefetch processes to make the resource monitoring check more
  // accurate.
  return !base::Contains(prerender_process_hosts_, process_host);
}

void NoStatePrefetchManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t erased = prerender_process_hosts_.erase(host);
  host->RemoveObserver(this);
  DCHECK_EQ(1u, erased);
}

base::WeakPtr<NoStatePrefetchManager> NoStatePrefetchManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void NoStatePrefetchManager::ClearPrefetchInformationForTesting() {
  prefetches_.clear();
}

std::unique_ptr<NoStatePrefetchHandle>
NoStatePrefetchManager::StartPrefetchingWithPreconnectFallbackForTesting(
    Origin origin,
    const GURL& url,
    const std::optional<url::Origin>& initiator_origin) {
  return StartPrefetchingWithPreconnectFallback(
      origin, url, content::Referrer(), initiator_origin, gfx::Rect(), nullptr);
}

void NoStatePrefetchManager::SetNoStatePrefetchContentsFactoryForTest(
    NoStatePrefetchContents::Factory* no_state_prefetch_contents_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  no_state_prefetch_contents_factory_.reset(no_state_prefetch_contents_factory);
}

}  // namespace prerender
