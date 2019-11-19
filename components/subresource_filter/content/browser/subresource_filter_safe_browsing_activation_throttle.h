// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {

class SubresourceFilterClient;

// Enum representing a position in the redirect chain. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class RedirectPosition {
  kOnly = 0,
  kFirst = 1,
  kMiddle = 2,
  kLast = 3,
  kMaxValue = kLast
};

// Navigation throttle responsible for activating subresource filtering on page
// loads that match the SUBRESOURCE_FILTER Safe Browsing list.
class SubresourceFilterSafeBrowsingActivationThrottle
    : public content::NavigationThrottle,
      public base::SupportsWeakPtr<
          SubresourceFilterSafeBrowsingActivationThrottle> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottle(
      content::NavigationHandle* handle,
      SubresourceFilterClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  ~SubresourceFilterSafeBrowsingActivationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  void OnCheckUrlResultOnUI(
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);

 private:
  // Highest priority config for a check result.
  struct ConfigResult {
    Configuration config;
    bool warning;
    bool matched_valid_configuration;
    ActivationList matched_list;

    ConfigResult(Configuration config,
                 bool warning,
                 bool matched_valid_configuration,
                 ActivationList matched_list);
    ~ConfigResult();
    ConfigResult();
    ConfigResult(const ConfigResult& result);
  };
  void CheckCurrentUrl();
  void NotifyResult();

  void LogMetricsOnChecksComplete(ActivationList matched_list,
                                  ActivationDecision decision,
                                  mojom::ActivationLevel level) const;
  bool HasFinishedAllSafeBrowsingChecks() const;
  // Gets the configuration with the highest priority among those activated.
  // Returns it, or none if no valid activated configurations.
  ConfigResult GetHighestPriorityConfiguration(
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);
  // Gets the ActivationDecision for the given Configuration.
  // Returns it, or ACTIVATION_CONDITIONS_NOT_MET if no Configuration.
  ActivationDecision GetActivationDecision(
      const std::vector<ConfigResult>& configs,
      ConfigResult* selected_config);

  // Returns whether a main-frame navigation satisfies the activation
  // |conditions| of a given configuration, except for |priority|.
  bool DoesMainFrameURLSatisfyActivationConditions(
      const Configuration::ActivationConditions& conditions,
      ActivationList matched_list) const;

  std::vector<SubresourceFilterSafeBrowsingClient::CheckResult> check_results_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<SubresourceFilterSafeBrowsingClient,
                  base::OnTaskRunnerDeleter>
      database_client_;

  // Must outlive this class.
  SubresourceFilterClient* client_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing safe browsing checks.
  bool deferring_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_
