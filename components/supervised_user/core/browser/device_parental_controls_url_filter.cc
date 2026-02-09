// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"

#include <memory>
#include <utility>

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace supervised_user {

DeviceParentalControlsUrlFilter::DeviceParentalControlsUrlFilter(
    DeviceParentalControls& device_parental_controls,
    std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client)
    : device_parental_controls_(device_parental_controls),
      async_url_checker_(std::make_unique<safe_search_api::URLChecker>(
          std::move(url_checker_client))) {
  device_parental_controls_subscription_ =
      device_parental_controls.Subscribe(base::BindRepeating(
          &DeviceParentalControlsUrlFilter::OnDeviceParentalControlsChanged,
          base::Unretained(this)));
}
DeviceParentalControlsUrlFilter::~DeviceParentalControlsUrlFilter() = default;

WebFilterType DeviceParentalControlsUrlFilter::GetWebFilterType() const {
  if (!device_parental_controls_->IsEnabled()) {
    return WebFilterType::kDisabled;
  }

  return device_parental_controls_->IsWebFilteringEnabled()
             ? WebFilterType::kTryToBlockMatureSites
             : WebFilterType::kAllowAllSites;
}

WebFilteringResult DeviceParentalControlsUrlFilter::GetFilteringBehavior(
    const GURL& url) const {
  // All checks without the remote Safe Sites service return a kAllow
  // classification, even if the filter is disabled.
  return {url, FilteringBehavior::kAllow,
          GetWebFilterType() == WebFilterType::kDisabled
              ? FilteringBehaviorReason::FILTER_DISABLED
              : FilteringBehaviorReason::DEFAULT};
}

void DeviceParentalControlsUrlFilter::GetFilteringBehavior(
    const GURL& url,
    bool skip_manual_parent_filter,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) {
  GetFilteringBehavior(url, std::move(callback), options);
}
void DeviceParentalControlsUrlFilter::GetFilteringBehaviorForSubFrame(
    const GURL& url,
    const GURL& main_frame_url,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) {
  // This filter treats subframes just like top-level frames.
  GetFilteringBehavior(url, std::move(callback), options);
}

void DeviceParentalControlsUrlFilter::GetFilteringBehavior(
    const GURL& url,
    WebFilteringResult::Callback callback,
    const WebFilterMetricsOptions& options) {
  callback = WrapCallbackWithUrlServiceMetrics(std::move(callback), options);

  // Only run the async checker if the web filter is actually enabled.
  if (GetWebFilterType() == WebFilterType::kTryToBlockMatureSites) {
    async_url_checker_->CheckURL(
        url_matcher::util::Normalize(url),
        WebFilteringResult::BindUrlCheckerCallback(
            std::move(callback), url, InterstitialMode::kLearnMoreInterstitial));
    return;
  }

  // Fall back to the default, always-allow and static (non-RPC) behavior.
  WebFilteringResult result = GetFilteringBehavior(url);
  CHECK(result.IsAllowed());
  std::move(callback).Run(result);
}

void DeviceParentalControlsUrlFilter::OnDeviceParentalControlsChanged(
    const DeviceParentalControls& controls) const {
  NotifyUrlFilteringDelegateChanged();
}

std::string_view DeviceParentalControlsUrlFilter::GetName() const {
  // LINT.IfChange(device_parental_controls_url_filtering_delegate)
  static constexpr std::string_view kName = "Device";
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/histograms.xml:device_parental_controls_url_filtering_delegate)
  return kName;
}

}  // namespace supervised_user
