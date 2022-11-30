// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"

#include "base/observer_list.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace subresource_filter {

SubresourceFilterObserverManager::SubresourceFilterObserverManager(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SubresourceFilterObserverManager>(
          *web_contents) {}

SubresourceFilterObserverManager::~SubresourceFilterObserverManager() {
  for (auto& observer : observers_)
    observer.OnSubresourceFilterGoingAway();
}

void SubresourceFilterObserverManager::AddObserver(
    SubresourceFilterObserver* observer) {
  observers_.AddObserver(observer);
}

void SubresourceFilterObserverManager::RemoveObserver(
    SubresourceFilterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SubresourceFilterObserverManager::NotifySafeBrowsingChecksComplete(
    content::NavigationHandle* navigation_handle,
    const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  for (auto& observer : observers_) {
    observer.OnSafeBrowsingChecksComplete(navigation_handle, result);
  }
}

void SubresourceFilterObserverManager::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const mojom::ActivationState& activation_state) {
  for (auto& observer : observers_) {
    observer.OnPageActivationComputed(navigation_handle, activation_state);
  }
}

void SubresourceFilterObserverManager::NotifyChildFrameNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  for (auto& observer : observers_)
    observer.OnChildFrameNavigationEvaluated(navigation_handle, load_policy);
}

void SubresourceFilterObserverManager::NotifyIsAdFrameChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_frame) {
  for (auto& observer : observers_)
    observer.OnIsAdFrameChanged(render_frame_host, is_ad_frame);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SubresourceFilterObserverManager);

}  // namespace subresource_filter
