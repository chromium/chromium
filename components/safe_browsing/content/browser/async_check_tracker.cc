// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"

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
  // TODO(crbug.com/1501194): Implement this function when
  // async Safe Browsing check is enabled.
  return resource.IsMainPageLoadPendingWithSyncCheck();
}

AsyncCheckTracker::AsyncCheckTracker(content::WebContents* web_contents,
                                     scoped_refptr<BaseUIManager> ui_manager)
    : content::WebContentsUserData<AsyncCheckTracker>(*web_contents),
      content::WebContentsObserver(web_contents),
      ui_manager_(std::move(ui_manager)) {}

AsyncCheckTracker::~AsyncCheckTracker() {
  if (!base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(pending_checker_));
  }
}

void AsyncCheckTracker::TransferUrlChecker(
    std::unique_ptr<UrlCheckerOnSB> checker) {
  pending_checker_ = std::move(checker);
  pending_checker_->SwapCompleteCallback(base::BindRepeating(
      &AsyncCheckTracker::PendingCheckerCompleted, GetWeakPtr()));
}

void AsyncCheckTracker::PendingCheckerCompleted(
    UrlCheckerOnSB::OnCompleteCheckResult result) {
  // TODO(crbug.com/1501194): Add a field in result to check
  // if load_post_commit_error_page is false.
  if (!result.proceed) {
    show_interstitial_after_finish_navigation_ = true;
  }
}

void AsyncCheckTracker::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument() ||
      !handle->HasCommitted()) {
    return;
  }

  if (!show_interstitial_after_finish_navigation_) {
    return;
  }
  // Reset immediately. If resource is not found, we don't retry. The resource
  // may be removed for other reasons.
  show_interstitial_after_finish_navigation_ = false;

  // Fields in `resource` is filled in by the call to
  // GetSeverestThreatForNavigation.
  UnsafeResource resource;
  ThreatSeverity severity =
      ui_manager_->GetSeverestThreatForNavigation(handle, resource);
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
  // Calling DisplayBlockingPage instead of StartDisplayingBlockingPage,
  // because when we decide that post commit error page should be
  // displayed, we already go through the checks in
  // StartDisplayingBlockingPage.
  ui_manager_->DisplayBlockingPage(resource);
}

bool AsyncCheckTracker::HasPendingCheckerForTesting() {
  return !!pending_checker_;
}

base::WeakPtr<AsyncCheckTracker> AsyncCheckTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
