// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_page_event_adapter.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace thumbnails {

namespace {

bool IsMainFrame(content::RenderFrameHost* render_frame_host) {
  return !render_frame_host->GetParent();
}

}  // namespace

ThumbnailPageEventAdapter::ThumbnailPageEventAdapter(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

ThumbnailPageEventAdapter::~ThumbnailPageEventAdapter() = default;

void ThumbnailPageEventAdapter::AddObserver(ThumbnailPageObserver* observer) {
  observers_.AddObserver(observer);
}

void ThumbnailPageEventAdapter::RemoveObserver(
    ThumbnailPageObserver* observer) {
  if (observers_.HasObserver(observer))
    observers_.RemoveObserver(observer);
}

void ThumbnailPageEventAdapter::OnVisibilityChanged(
    content::Visibility visibility) {
  const bool visible = visibility == content::Visibility::VISIBLE;
  for (auto& observer : observers_)
    observer.VisibilityChanged(visible);
}

void ThumbnailPageEventAdapter::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    const GURL& url = navigation_handle->GetWebContents()->GetVisibleURL();
    for (auto& observer : observers_)
      observer.TopLevelNavigationStarted(url);
  }
}

void ThumbnailPageEventAdapter::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    const GURL& url = navigation_handle->GetWebContents()->GetVisibleURL();
    for (auto& observer : observers_)
      observer.TopLevelNavigationStarted(url);
  }
}

void ThumbnailPageEventAdapter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    const GURL& url = navigation_handle->GetWebContents()->GetVisibleURL();
    for (auto& observer : observers_)
      observer.TopLevelNavigationEnded(url);
  }
}

void ThumbnailPageEventAdapter::DocumentAvailableInMainFrame() {
  for (auto& observer : observers_)
    observer.PageLoadStarted();
}

void ThumbnailPageEventAdapter::DidFirstVisuallyNonEmptyPaint() {
  for (auto& observer : observers_)
    observer.PagePainted();
}

void ThumbnailPageEventAdapter::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (IsMainFrame(render_frame_host)) {
    for (auto& observer : observers_)
      observer.PageLoadFinished();
  }
}

void ThumbnailPageEventAdapter::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  if (IsMainFrame(render_frame_host)) {
    for (auto& observer : observers_)
      observer.PageLoadFinished();
  }
}

void ThumbnailPageEventAdapter::NavigationStopped() {
  const GURL& url = web_contents()->GetVisibleURL();
  for (auto& observer : observers_) {
    observer.TopLevelNavigationEnded(url);
    observer.PageLoadFinished();
  }
}

void ThumbnailPageEventAdapter::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  is_unloading_ = true;
}

void ThumbnailPageEventAdapter::BeforeUnloadDialogCancelled() {
  is_unloading_ = false;
}

}  // namespace thumbnails
