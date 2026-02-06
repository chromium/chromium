// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"

namespace supervised_user {

namespace {

// LINT.IfChange(url_filtering_service)
static const char kUrlFilteringServiceComponentName[] = "All";
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/histograms.xml:url_filtering_service)

// Combines two callbacks into one.
void CombineCallbacks(WebFilteringResult::Callback a,
                      WebFilteringResult::Callback b,
                      WebFilteringResult result) {
  std::move(a).Run(result);
  std::move(b).Run(result);
}

FilteringBehavior GetBehaviorFromSafeSearchClassification(
    safe_search_api::Classification classification) {
  switch (classification) {
    case safe_search_api::Classification::SAFE:
      return FilteringBehavior::kAllow;
    case safe_search_api::Classification::UNSAFE:
      return FilteringBehavior::kBlock;
  }
  NOTREACHED();
}

void UrlCheckerCallback(WebFilteringResult::Callback callback,
                        GURL request_url,
                        InterstitialMode interstitial_mode,
                        const GURL& checked_url,
                        safe_search_api::Classification classification,
                        safe_search_api::ClassificationDetails details) {
  std::move(callback).Run(
      {.url = request_url,
       .behavior = GetBehaviorFromSafeSearchClassification(classification),
       .reason = supervised_user::FilteringBehaviorReason::ASYNC_CHECKER,
       .async_check_details = details,
       .interstitial_mode = interstitial_mode});
}

// Aggregates the web filter types from two delegates. The settings are rated in
// the following order and the most restrictive setting is returned:
// 1. kTryToBlockMatureSites
// 2. kCertainSites
// 3. kAllowAllSites
// 4. kDisabled
WebFilterType AggregateWebFilterType(const UrlFilteringDelegate& first,
                                     const UrlFilteringDelegate& second) {
  WebFilterType first_web_filter_type = first.GetWebFilterType();
  WebFilterType second_web_filter_type = second.GetWebFilterType();

  if (first_web_filter_type == WebFilterType::kTryToBlockMatureSites ||
      second_web_filter_type == WebFilterType::kTryToBlockMatureSites) {
    return WebFilterType::kTryToBlockMatureSites;
  }
  if (first_web_filter_type == WebFilterType::kCertainSites ||
      second_web_filter_type == WebFilterType::kCertainSites) {
    return WebFilterType::kCertainSites;
  }
  if (first_web_filter_type == WebFilterType::kAllowAllSites ||
      second_web_filter_type == WebFilterType::kAllowAllSites) {
    return WebFilterType::kAllowAllSites;
  }
  return WebFilterType::kDisabled;
}

// Helper utility that groups filtering behavior arguments together and
// implements the logic for combining results for main frame URLs (by actually
// just serializing the calls to delegates, since two systems enabled at one
// time are expected to be the edge case so there's little need to optimize).
// With all but last arguments bound, this is exactly
// WebFilteringResult::Callback.
void OnFirstFilteringBehaviorResult(
    const GURL& url,
    bool skip_manual_parent_filter,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options,
    base::WeakPtr<UrlFilteringDelegate> fallback_delegate,
    WebFilteringResult result) {
  // Also use the first result if the fallback delegate is not available.
  if (result.IsBlocked() || !fallback_delegate) {
    std::move(callback).Run(result);
    return;
  }
  fallback_delegate->GetFilteringBehavior(url, skip_manual_parent_filter,
                                          std::move(callback), options);
}

void OnFirstFilteringBehaviorResultForSubFrame(
    const GURL& url,
    const GURL& main_frame_url,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options,
    base::WeakPtr<UrlFilteringDelegate> fallback_delegate,
    WebFilteringResult result) {
  // Also use the first result if the fallback delegate is not available.
  if (result.IsBlocked() || !fallback_delegate) {
    std::move(callback).Run(result);
    return;
  }
  fallback_delegate->GetFilteringBehaviorForSubFrame(
      url, main_frame_url, std::move(callback), options);
}

std::string GetTopLevelFilteringResultHistogramName(
    std::string_view component_name,
    const WebFilterMetricsOptions& options) {
  static const char name_template[] =
      "SupervisedUsers.$1.TopLevelFilteringResult$2";
  return base::ReplaceStringPlaceholders(
      name_template,
      {std::string(component_name),
       GetFilteringContextName(options.filtering_context)},
      /*offsets=*/nullptr);
}

// Emits metrics about the filtering process, either at delegate or service
// level, as identified by the `component_name` argument.
void EmitMetrics(WebFilterType web_filter_type,
                 std::string_view component_name,
                 WebFilterMetricsOptions options,
                 WebFilteringResult result) {
  if (web_filter_type == WebFilterType::kDisabled) {
    return;
  }
  base::UmaHistogramSparse(
      GetTopLevelFilteringResultHistogramName(component_name, options),
      static_cast<int>(result.ToTopLevelResult()));
}
}  // namespace

// Creates a callback for safe search api that will invoke `callback` argument
// with check result.
safe_search_api::URLChecker::CheckCallback
WebFilteringResult::BindUrlCheckerCallback(Callback callback,
                                           const GURL& requested_url,
                                           InterstitialMode interstitial_mode) {
  return base::BindOnce(&UrlCheckerCallback, std::move(callback), requested_url,
                        interstitial_mode);
}

SupervisedUserFilterTopLevelResult WebFilteringResult::ToTopLevelResult()
    const {
  switch (behavior) {
    case FilteringBehavior::kAllow:
      return SupervisedUserFilterTopLevelResult::kAllow;
    case FilteringBehavior::kBlock:
      switch (reason) {
        case FilteringBehaviorReason::ASYNC_CHECKER:
          return SupervisedUserFilterTopLevelResult::kBlockSafeSites;
        case FilteringBehaviorReason::MANUAL:
          return SupervisedUserFilterTopLevelResult::kBlockManual;
        case FilteringBehaviorReason::DEFAULT:
          return SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist;
        case FilteringBehaviorReason::FILTER_DISABLED:
          NOTREACHED() << "Histograms must not be generated when the "
                          "supervised user URL filter is turned off.";
      }
    case FilteringBehavior::kInvalid:
      NOTREACHED();
  }
  NOTREACHED();
}

SupervisedUserUrlFilteringService::SupervisedUserUrlFilteringService(
    const SupervisedUserService& supervised_user_service,
    std::unique_ptr<UrlFilteringDelegate> device_parental_controls_url_filter)
    : supervised_user_service_(supervised_user_service),
      device_parental_controls_url_filter_(
          std::move(device_parental_controls_url_filter)) {
  family_link_url_filter_observation_.Observe(
      supervised_user_service_->GetURLFilter());
  device_parental_controls_url_filter_observation_.Observe(
      device_parental_controls_url_filter_.get());
}
SupervisedUserUrlFilteringService::~SupervisedUserUrlFilteringService() =
    default;

WebFilterType SupervisedUserUrlFilteringService::GetWebFilterType() const {
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    return AggregateWebFilterType(*device_parental_controls_url_filter_,
                                  *supervised_user_service_->GetURLFilter());
  }
  return supervised_user_service_->GetURLFilter()->GetWebFilterType();
}

WebFilteringResult SupervisedUserUrlFilteringService::GetFilteringBehavior(
    const GURL& url) const {
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    WebFilteringResult device_filtering_result =
        device_parental_controls_url_filter_->GetFilteringBehavior(url);
    CHECK(device_filtering_result.IsAllowed())
        << "Device filtering always passes synchronous checks.";
  }
  return supervised_user_service_->GetURLFilter()->GetFilteringBehavior(url);
}

void SupervisedUserUrlFilteringService::GetFilteringBehavior(
    const GURL& url,
    bool skip_manual_parent_filter,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) const {
  callback =
      base::BindOnce(&CombineCallbacks,
                     base::BindOnce(&EmitMetrics, GetWebFilterType(),
                                    kUrlFilteringServiceComponentName, options),
                     std::move(callback));
  callback = base::BindOnce(
      &CombineCallbacks, std::move(callback),
      base::BindOnce(&SupervisedUserUrlFilteringService::NotifyUrlChecked,
                     weak_ptr_factory_.GetWeakPtr()));

  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    device_parental_controls_url_filter_->GetFilteringBehavior(
        url, skip_manual_parent_filter,
        base::BindOnce(&OnFirstFilteringBehaviorResult, url,
                       skip_manual_parent_filter, std::move(callback), options,
                       supervised_user_service_->GetURLFilter()->GetWeakPtr()),
        options);
    return;
  }
  supervised_user_service_->GetURLFilter()->GetFilteringBehavior(
      url, skip_manual_parent_filter, std::move(callback), options);
}

// Version of the above method that for use in subframe context.
void SupervisedUserUrlFilteringService::GetFilteringBehaviorForSubFrame(
    const GURL& url,
    const GURL& main_frame_url,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) const {
  callback =
      base::BindOnce(&CombineCallbacks,
                     base::BindOnce(&EmitMetrics, GetWebFilterType(),
                                    kUrlFilteringServiceComponentName, options),
                     std::move(callback));
  callback = base::BindOnce(
      &CombineCallbacks, std::move(callback),
      base::BindOnce(&SupervisedUserUrlFilteringService::NotifyUrlChecked,
                     weak_ptr_factory_.GetWeakPtr()));
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    device_parental_controls_url_filter_->GetFilteringBehaviorForSubFrame(
        url, main_frame_url,
        base::BindOnce(&OnFirstFilteringBehaviorResultForSubFrame, url,
                       main_frame_url, std::move(callback), options,
                       supervised_user_service_->GetURLFilter()->GetWeakPtr()),
        options);
    return;
  }

  supervised_user_service_->GetURLFilter()->GetFilteringBehaviorForSubFrame(
      url, main_frame_url, std::move(callback), options);
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

base::WeakPtr<UrlFilteringDelegate> UrlFilteringDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WebFilteringResult::Callback
UrlFilteringDelegate::WrapCallbackWithUrlServiceMetrics(
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) const {
  return base::BindOnce(
      &CombineCallbacks,
      base::BindOnce(&EmitMetrics, GetWebFilterType(), GetName(), options),
      std::move(callback));
}

UrlFilteringDelegateObserver::~UrlFilteringDelegateObserver() = default;

}  // namespace supervised_user
