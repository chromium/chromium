// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include "base/feature_list.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"

namespace supervised_user {

namespace {
// Combines two callbacks into one.
void CombineCallbacks(WebFilteringResult::Callback a,
                      WebFilteringResult::Callback b,
                      WebFilteringResult result) {
  std::move(a).Run(result);
  std::move(b).Run(result);
}
}  // namespace

SupervisedUserUrlFilteringService::SupervisedUserUrlFilteringService(
    const SupervisedUserService& supervised_user_service)
    : supervised_user_service_(supervised_user_service) {
  family_link_url_filter_observation_.Observe(
      supervised_user_service_->GetURLFilter());
}
SupervisedUserUrlFilteringService::~SupervisedUserUrlFilteringService() =
    default;

WebFilterType SupervisedUserUrlFilteringService::GetWebFilterType() const {
  return supervised_user_service_->GetURLFilter()->GetWebFilterType();
}

WebFilteringResult SupervisedUserUrlFilteringService::GetFilteringBehavior(
    const GURL& url) const {
  return supervised_user_service_->GetURLFilter()->GetFilteringBehavior(url);
}

void SupervisedUserUrlFilteringService::GetFilteringBehavior(
    const GURL& url,
    bool skip_manual_parent_filter,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) const {
  WebFilteringResult::Callback combined_callback = base::BindOnce(
      &CombineCallbacks, std::move(callback),
      base::BindOnce(&SupervisedUserUrlFilteringService::NotifyUrlChecked,
                     weak_ptr_factory_.GetWeakPtr()));
  supervised_user_service_->GetURLFilter()->GetFilteringBehavior(
      url, skip_manual_parent_filter, std::move(combined_callback), options);
}

// Version of the above method that for use in subframe context.
void SupervisedUserUrlFilteringService::GetFilteringBehaviorForSubFrame(
    const GURL& url,
    const GURL& main_frame_url,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) const {
  WebFilteringResult::Callback combined_callback = base::BindOnce(
      &CombineCallbacks, std::move(callback),
      base::BindOnce(&SupervisedUserUrlFilteringService::NotifyUrlChecked,
                     weak_ptr_factory_.GetWeakPtr()));
  supervised_user_service_->GetURLFilter()->GetFilteringBehaviorForSubFrame(
      url, main_frame_url, std::move(combined_callback), options);
}

void SupervisedUserUrlFilteringService::NotifyUrlChecked(
    WebFilteringResult result) const {
  for (Observer& observer : observer_list_) {
    observer.OnUrlChecked(result);
  }
}

void SupervisedUserUrlFilteringService::OnUrlFilteringDelegateChanged(
    const UrlFilteringDelegate& delegate) const {
  for (Observer& observer : observer_list_) {
    observer.OnUrlFilteringServiceChanged();
  }
}
void SupervisedUserUrlFilteringService::OnUrlChecked(
    const UrlFilteringDelegate& delegate,
    WebFilteringResult result) const {
  for (Observer& observer : observer_list_) {
    observer.OnUrlChecked(result);
  }
}

void SupervisedUserUrlFilteringService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void SupervisedUserUrlFilteringService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
SupervisedUserUrlFilteringService::Observer::~Observer() = default;

UrlFilteringDelegate::UrlFilteringDelegate() = default;
UrlFilteringDelegate::~UrlFilteringDelegate() = default;

void UrlFilteringDelegate::NotifyUrlFilteringDelegateChanged() const {
  for (auto& observer : observers_) {
    observer.OnUrlFilteringDelegateChanged(*this);
  }
}

void UrlFilteringDelegate::NotifyUrlChecked(WebFilteringResult result) const {
  for (auto& observer : observers_) {
    observer.OnUrlChecked(*this, result);
  }
}

void UrlFilteringDelegate::AddObserver(UrlFilteringDelegateObserver* observer) {
  observers_.AddObserver(observer);
}
void UrlFilteringDelegate::RemoveObserver(
    UrlFilteringDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

UrlFilteringDelegateObserver::~UrlFilteringDelegateObserver() = default;

}  // namespace supervised_user
