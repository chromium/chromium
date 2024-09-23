// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_PAGE_ACTIVATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_PAGE_ACTIVATION_THROTTLE_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {

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
class SafeBrowsingPageActivationThrottle final
    : public content::NavigationThrottle {
 public:
  // Interface that allows the client of this class to adjust activation
  // decisions if/as desired.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the initial activation decision has been computed by the
    // page activation throttle. Returns the effective activation for this
    // navigation.
    //
    // Note: `decision` is guaranteed to be non-nullptr, and can be modified by
    // this method if any decision changes.
    //
    // Precondition: The navigation must be a root frame navigation.
    virtual mojom::ActivationLevel OnPageActivationComputed(
        content::NavigationHandle* navigation_handle,
        mojom::ActivationLevel initial_activation_level,
        ActivationDecision* decision) = 0;
  };

  // `delegate` is allowed to be null, in which case the client creating this
  // throttle will not be able to adjust activation decisions made by the
  // throttle.
  SafeBrowsingPageActivationThrottle(
      content::NavigationHandle* handle,
      Delegate* delegate,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  SafeBrowsingPageActivationThrottle(
      const SafeBrowsingPageActivationThrottle&) = delete;
  SafeBrowsingPageActivationThrottle& operator=(
      const SafeBrowsingPageActivationThrottle&) = delete;

  ~SafeBrowsingPageActivationThrottle() override;

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
  ActivationDecision GetActivationDecision(const ConfigResult& configs);

  // Returns whether a root-frame navigation satisfies the activation
  // |conditions| of a given configuration, except for |priority|.
  bool DoesRootFrameURLSatisfyActivationConditions(
      const Configuration::ActivationConditions& conditions,
      ActivationList matched_list) const;

  std::vector<SubresourceFilterSafeBrowsingClient::CheckResult> check_results_;

  std::unique_ptr<SubresourceFilterSafeBrowsingClient> database_client_;

  // May be null. If non-null, must outlive this class.
  raw_ptr<Delegate> delegate_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing safe browsing checks.
  bool deferring_ = false;

  base::WeakPtrFactory<SafeBrowsingPageActivationThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_PAGE_ACTIVATION_THROTTLE_H_
