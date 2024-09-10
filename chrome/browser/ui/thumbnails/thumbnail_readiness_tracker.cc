// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"

#include <utility>

#include "content/public/browser/navigation_handle.h"

namespace {

bool NavigationShouldInvalidateThumbnail(
    content::NavigationHandle* navigation) {
  // Ignore subframe navigations.
  if (!navigation->IsInPrimaryMainFrame())
    return false;

  // Some navigations change the tab URL but don't create a new
  // document. They aren't considered loading; onload is never triggered
  // after they complete. They shouldn't affect the thumbnail.
  //
  // See crbug.com/1120940 for why this is necessary.
  if (navigation->IsSameDocument())
    return false;

  return true;
}

}  // namespace

ThumbnailReadinessTracker::ThumbnailReadinessTracker(
    content::WebContents* web_contents,
    ReadinessChangeCallback callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}

ThumbnailReadinessTracker::~ThumbnailReadinessTracker() = default;

void ThumbnailReadinessTracker::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!NavigationShouldInvalidateThumbnail(navigation_handle))
    return;

  pending_navigation_ = navigation_handle;
  UpdateReadiness(Readiness::kNotReady);
}

void ThumbnailReadinessTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only respond to the last navigation handled by DidStartNavigation.
  // Others may be unimportant or outdated.
  if (navigation_handle != pending_navigation_)
    return;

  pending_navigation_ = nullptr;
  UpdateReadiness(Readiness::kReadyForInitialCapture);

  if (last_readiness_ > Readiness::kReadyForInitialCapture)
    return;
  UpdateReadiness(Readiness::kReadyForInitialCapture);
}

void ThumbnailReadinessTracker::DocumentOnLoadCompletedInPrimaryMainFrame() {
  UpdateReadiness(Readiness::kReadyForFinalCapture);
}

void ThumbnailReadinessTracker::WebContentsDestroyed() {
  pending_navigation_ = nullptr;
  UpdateReadiness(Readiness::kNotReady);
}

void ThumbnailReadinessTracker::WasDiscarded() {
  // A new ThumbnailTabHelper and its associated tracker is created every time
  // a new tab WebContents is created. A new tracker starts at the kNotReady
  // state. Set this explicitly during discard to reset readiness for the new
  // discard implementation that retains the tab's WebContents (and consequently
  // does not recreate the tab helper).
  UpdateReadiness(Readiness::kNotReady);
}

void ThumbnailReadinessTracker::UpdateReadiness(Readiness readiness) {
  if (readiness == last_readiness_)
    return;

  // If the WebContents is closing, it shouldn't be captured.
  if (web_contents()->IsBeingDestroyed())
    readiness = Readiness::kNotReady;

  last_readiness_ = readiness;
  callback_.Run(readiness);
}
