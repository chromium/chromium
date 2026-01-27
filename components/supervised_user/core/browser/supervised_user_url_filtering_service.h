// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

// Forward declared until all delegates are no longer owned by it.
class SupervisedUserService;

// Represents the result of url filtering request.
struct WebFilteringResult {
  using Callback = base::OnceCallback<void(WebFilteringResult result)>;

  // The URL that was subject to filtering
  GURL url;
  // How the URL should be handled.
  FilteringBehavior behavior;
  // Why the URL is handled as indicated in `behavior`.
  FilteringBehaviorReason reason;
  // Details of asynchronous check if it was performed, otherwise empty.
  std::optional<safe_search_api::ClassificationDetails> async_check_details;

  bool IsFromManualList() const {
    return reason == FilteringBehaviorReason::MANUAL;
  }
  bool IsFromDefaultSetting() const {
    return reason == FilteringBehaviorReason::DEFAULT;
  }
  bool IsAllowedBecauseOfDisabledFilter() const {
    return reason == FilteringBehaviorReason::FILTER_DISABLED &&
           behavior == FilteringBehavior::kAllow;
  }

  // True when the result of the classification means that the url is safe.
  // See `::IsClassificationSuccessful` for caveats.
  bool IsAllowed() const { return behavior == FilteringBehavior::kAllow; }
  // True when the result of the classification means that the url is not
  // safe. See `::IsClassificationSuccessful` for caveats.
  bool IsBlocked() const { return behavior == FilteringBehavior::kBlock; }

  // True when remote classification was successful. It's useful to understand
  // if the result is "allowed" because the classification succeeded, or
  // because it failed and the system uses a default classification.
  bool IsClassificationSuccessful() const {
    return !async_check_details.has_value() ||
           async_check_details->reason !=
               safe_search_api::ClassificationDetails::Reason::
                   kFailedUseDefault;
  }
};

// Interface for actual implementations of URL filtering logic. The outer
// service subscribes to individual delegates and forwards notifications to
// its own subscribers.
class UrlFilteringDelegate {
 public:
  virtual ~UrlFilteringDelegate();

  virtual WebFilterType GetWebFilterType() const = 0;
  virtual WebFilteringResult GetFilteringBehavior(const GURL& url) const = 0;

  // TODO(crbug.com/478188599): Declare const after url_checker_ clients are
  // owned in this service and passed to delegates.
  virtual void GetFilteringBehavior(const GURL& url,
                                    bool skip_manual_parent_filter,
                                    WebFilteringResult::Callback callback,
                                    const WebFilterMetricsOptions& options) = 0;
  // TODO(crbug.com/478188599): Declare const after url_checker_ clients are
  // owned in this service and passed to delegates.
  virtual void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options) = 0;
};

// Performs URL filtering workflows for supervised users, combining effects of
// subservices that define the status of these users.
class SupervisedUserUrlFilteringService : public KeyedService {
 public:
  SupervisedUserUrlFilteringService(
      const SupervisedUserService& supervised_user_service,
      const FamilyLinkSettingsService& family_link_settings_service);
  ~SupervisedUserUrlFilteringService() override;
  SupervisedUserUrlFilteringService(const SupervisedUserUrlFilteringService&) =
      delete;
  SupervisedUserUrlFilteringService& operator=(
      const SupervisedUserUrlFilteringService&) = delete;

  // Returns the type of web filter that is applied to the current profile.
  WebFilterType GetWebFilterType() const;

  // Returns the filtering status for a given URL without any remote checks.
  WebFilteringResult GetFilteringBehavior(const GURL& url) const;

  // Version of the above method that adds asynchronous checks against a
  // remote service if GetFilteringBehavior(.) was inconclusive.
  // `skip_manual_parent_filter` will ignore result from
  // GetFilteringBehavior(url) even if it was conclusive.
  void GetFilteringBehavior(
      const GURL& url,
      bool skip_manual_parent_filter,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options = WebFilterMetricsOptions()) const;

  // Version of the above method that for use in subframe context.
  void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options = WebFilterMetricsOptions()) const;

 private:
  // Provides access to legacy way of resolving URL filtering.
  raw_ref<const SupervisedUserService> supervised_user_service_;

  // Provides access to parental controls settings from Family Link.
  raw_ref<const FamilyLinkSettingsService> family_link_settings_service_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
