// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

using security_interstitials::UnsafeResource;

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(AsyncCheckTracker);

// static
AsyncCheckTracker* AsyncCheckTracker::GetOrCreateForWebContents(
    content::WebContents* web_contents,
    scoped_refptr<BaseUIManager> ui_manager) {
  CHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  AsyncCheckTracker::CreateForWebContents(web_contents, std::move(ui_manager));
  return AsyncCheckTracker::FromWebContents(web_contents);
}

// static
bool AsyncCheckTracker::IsMainPageLoadPending(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  if (web_contents && AsyncCheckTracker::FromWebContents(web_contents) &&
      resource.navigation_id.has_value() &&
      base::FeatureList::IsEnabled(kSafeBrowsingAsyncRealTimeCheck)) {
    // If async check is enabled, whether the main page load is pending cannot
    // be solely determined by the fields in resource. The page load may or may
    // not be pending, depending on when the async check completes.
    return AsyncCheckTracker::FromWebContents(web_contents)
        ->IsNavigationPending(resource.navigation_id.value());
  }
  return resource.IsMainPageLoadPendingWithSyncCheck();
}

AsyncCheckTracker::AsyncCheckTracker(content::WebContents* web_contents,
                                     scoped_refptr<BaseUIManager> ui_manager)
    : content::WebContentsUserData<AsyncCheckTracker>(*web_contents),
      content::WebContentsObserver(web_contents),
      ui_manager_(std::move(ui_manager)) {}

AsyncCheckTracker::~AsyncCheckTracker() {
  DeletePendingCheckers(/*excluded_navigation_id=*/absl::nullopt);
}

void AsyncCheckTracker::TransferUrlChecker(
    std::unique_ptr<UrlCheckerOnSB> checker) {
  absl::optional<int64_t> navigation_id = checker->navigation_id();
  CHECK(navigation_id.has_value());
  int64_t id = navigation_id.value();
  // If there is an old checker with the same navigation_id, we should delete
  // the old one since the navigation only holds one url_loader and it has
  // decided to delete the old one.
  MaybeDeleteChecker(id);
  pending_checkers_[id] = std::move(checker);
  pending_checkers_[id]->SwapCompleteCallback(base::BindRepeating(
      &AsyncCheckTracker::PendingCheckerCompleted, GetWeakPtr(), id));
  base::UmaHistogramCounts10000("SafeBrowsing.AsyncCheck.PendingCheckersSize",
                                pending_checkers_.size());
}

void AsyncCheckTracker::PendingCheckerCompleted(
    int64_t navigation_id,
    UrlCheckerOnSB::OnCompleteCheckResult result) {
  if (!base::Contains(pending_checkers_, navigation_id)) {
    return;
  }
  if (result.has_post_commit_interstitial_skipped) {
    CHECK(!result.proceed);
    if (IsNavigationPending(navigation_id)) {
      show_interstitial_after_finish_navigation_ = true;
    } else {
      // If the navigation has already finished, show a warning immediately.
      MaybeDisplayBlockingPage(
          pending_checkers_[navigation_id]->GetRedirectChain(), navigation_id);
    }
  }
  if (!result.proceed || result.all_checks_completed) {
    // No need to keep the checker around if proceed is false. We
    // cannot delete the checker if all_checks_completed is false and
    // proceed is true, because PendingCheckerCompleted may be called multiple
    // times during server redirects.
    MaybeDeleteChecker(navigation_id);
  }
}

bool AsyncCheckTracker::IsNavigationPending(int64_t navigation_id) {
  return !base::Contains(committed_navigation_ids_, navigation_id);
}

void AsyncCheckTracker::DidFinishNavigation(content::NavigationHandle* handle) {
  int64_t navigation_id = handle->GetNavigationId();
  if (handle->HasCommitted() && !handle->IsSameDocument()) {
    // Do not filter out non primary main frame navigation because
    // `IsNavigationPending` may be called for these navigation. For example,
    // an async check is performed on the current WebContents (so
    // AsyncCheckTracker is created) and then a prerendered navigation starts
    // on the same WebContents.
    committed_navigation_ids_.insert(navigation_id);
  }
  base::UmaHistogramCounts10000(
      "SafeBrowsing.AsyncCheck.CommittedNavigationIdsSize",
      committed_navigation_ids_.size());

  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument() ||
      !handle->HasCommitted()) {
    return;
  }

  // If a new main page has committed, remove other checkers because we have
  // navigated away.
  DeletePendingCheckers(/*excluded_navigation_id=*/navigation_id);

  if (!show_interstitial_after_finish_navigation_) {
    return;
  }
  // Reset immediately. If resource is not found, we don't retry. The resource
  // may be removed for other reasons.
  show_interstitial_after_finish_navigation_ = false;

  MaybeDisplayBlockingPage(handle->GetRedirectChain(),
                           handle->GetNavigationId());
}

void AsyncCheckTracker::MaybeDisplayBlockingPage(
    const std::vector<GURL>& redirect_chain,
    int64_t navigation_id) {
  // Fields in `resource` is filled in by the call to
  // GetSeverestThreatForNavigation.
  UnsafeResource resource;
  ThreatSeverity severity = ui_manager_->GetSeverestThreatForRedirectChain(
      redirect_chain, navigation_id, resource);
  if (severity == std::numeric_limits<ThreatSeverity>::max() ||
      resource.threat_type == SBThreatType::SB_THREAT_TYPE_SAFE) {
    return;
  }
  auto* primary_main_frame = web_contents()->GetPrimaryMainFrame();
  resource.render_process_id = primary_main_frame->GetGlobalId().child_id;
  resource.render_frame_token = primary_main_frame->GetFrameToken().value();
  // The callback has already been run when BaseUIManager attempts to
  // trigger post commit error page, so there is no need to run again.
  resource.callback = base::DoNothing();
  // Post a task instead of calling DisplayBlockingPage directly, because
  // SecurityInterstitialTabHelper also listens to DidFinishNavigation. We
  // need to ensure that the tab helper has updated its state before calling
  // DisplayBlockingPage.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AsyncCheckTracker::DisplayBlockingPage,
                                GetWeakPtr(), std::move(resource)));
}

void AsyncCheckTracker::DisplayBlockingPage(UnsafeResource resource) {
  // Calling DisplayBlockingPage instead of StartDisplayingBlockingPage,
  // because when we decide that post commit error page should be
  // displayed, we already go through the checks in
  // StartDisplayingBlockingPage.
  ui_manager_->DisplayBlockingPage(resource);
}

void AsyncCheckTracker::MaybeDeleteChecker(int64_t navigation_id) {
  if (!base::Contains(pending_checkers_, navigation_id)) {
    return;
  }
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    pending_checkers_[navigation_id].reset();
  } else {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(pending_checkers_[navigation_id]));
  }
  pending_checkers_.erase(navigation_id);
}

void AsyncCheckTracker::DeletePendingCheckers(
    absl::optional<int64_t> excluded_navigation_id) {
  for (auto it = pending_checkers_.begin(); it != pending_checkers_.end();) {
    if (excluded_navigation_id.has_value() &&
        it->first == excluded_navigation_id.value()) {
      it++;
      continue;
    }
    if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
      it->second.reset();
    } else {
      content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                     std::move(it->second));
    }
    it = pending_checkers_.erase(it);
  }
}

size_t AsyncCheckTracker::PendingCheckersSizeForTesting() {
  return pending_checkers_.size();
}

base::WeakPtr<AsyncCheckTracker> AsyncCheckTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
