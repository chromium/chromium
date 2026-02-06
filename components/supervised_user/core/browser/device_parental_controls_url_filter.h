// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_URL_FILTER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_URL_FILTER_H_

#include "components/safe_search_api/url_checker.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

namespace supervised_user {

// Implementation of url filtering delegate that reads filtering configuration
// from the device without user context, and checks urls against remote service
// without end user credentials.
class DeviceParentalControlsUrlFilter : public UrlFilteringDelegate {
 public:
  DeviceParentalControlsUrlFilter(
      DeviceParentalControls& device_parental_controls,
      std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client);
  DeviceParentalControlsUrlFilter(const DeviceParentalControlsUrlFilter&) =
      delete;
  DeviceParentalControlsUrlFilter& operator=(
      const DeviceParentalControlsUrlFilter&) = delete;
  ~DeviceParentalControlsUrlFilter() override;

  // UrlFilteringDelegate:
  WebFilterType GetWebFilterType() const override;
  WebFilteringResult GetFilteringBehavior(const GURL& url) const override;
  void GetFilteringBehavior(const GURL& url,
                            bool skip_manual_parent_filter,
                            WebFilteringResult::Callback callback,
                            const WebFilterMetricsOptions& options) override;
  void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options) override;
  std::string_view GetName() const override;

 private:
  void OnDeviceParentalControlsChanged(
      const DeviceParentalControls& controls) const;

  // Actual implementation of filtering behavior.
  void GetFilteringBehavior(const GURL& url,
                            WebFilteringResult::Callback callback,
                            const WebFilterMetricsOptions& options);

  raw_ref<const DeviceParentalControls> device_parental_controls_;

  std::unique_ptr<safe_search_api::URLChecker> async_url_checker_;

  base::CallbackListSubscription device_parental_controls_subscription_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_URL_FILTER_H_
