// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/navigation_console_logger.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

using CheckResults =
    std::vector<SubresourceFilterSafeBrowsingClient::CheckResult>;

base::Optional<RedirectPosition> GetEnforcementRedirectPosition(
    const CheckResults& results) {
  // Safe cast since we have strict limits on HTTP redirects.
  int num_results = static_cast<int>(results.size());
  for (int i = num_results - 1; i >= 0; --i) {
    bool warning = false;
    ActivationList list = GetListForThreatTypeAndMetadata(
        results[i].threat_type, results[i].threat_metadata, &warning);
    if (!warning && list != ActivationList::NONE) {
      if (num_results == 1)
        return RedirectPosition::kOnly;
      if (i == 0)
        return RedirectPosition::kFirst;
      if (i == num_results - 1)
        return RedirectPosition::kLast;
      return RedirectPosition::kMiddle;
    }
  }
  return base::nullopt;
}

}  // namespace

SubresourceFilterSafeBrowsingActivationThrottle::
    SubresourceFilterSafeBrowsingActivationThrottle(
        content::NavigationHandle* handle,
        SubresourceFilterClient* client,
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager)
    : NavigationThrottle(handle),
      io_task_runner_(std::move(io_task_runner)),
      database_client_(new SubresourceFilterSafeBrowsingClient(
                           std::move(database_manager),
                           AsWeakPtr(),
                           io_task_runner_,
                           base::ThreadTaskRunnerHandle::Get()),
                       base::OnTaskRunnerDeleter(io_task_runner_)),
      client_(client) {
  DCHECK(handle->IsInMainFrame());

  CheckCurrentUrl();
  DCHECK(!check_results_.empty());
}

SubresourceFilterSafeBrowsingActivationThrottle::
    ~SubresourceFilterSafeBrowsingActivationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillRedirectRequest() {
  CheckCurrentUrl();
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillProcessResponse() {
  // No need to defer the navigation if the check already happened.
  if (HasFinishedAllSafeBrowsingChecks()) {
    NotifyResult();
    return PROCEED;
  }
  CHECK(!deferring_);
  deferring_ = true;
  defer_time_ = base::TimeTicks::Now();
  return DEFER;
}

const char*
SubresourceFilterSafeBrowsingActivationThrottle::GetNameForLogging() {
  return "SubresourceFilterSafeBrowsingActivationThrottle";
}

void SubresourceFilterSafeBrowsingActivationThrottle::OnCheckUrlResultOnUI(
    const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t request_id = result.request_id;
  DCHECK_LT(request_id, check_results_.size());

  auto& stored_result = check_results_.at(request_id);
  CHECK(!stored_result.finished);
  stored_result = result;

  UMA_HISTOGRAM_TIMES("SubresourceFilter.SafeBrowsing.TotalCheckTime",
                      base::TimeTicks::Now() - result.start_time);
  if (deferring_ && HasFinishedAllSafeBrowsingChecks()) {
    NotifyResult();

    deferring_ = false;
    Resume();
  }
}

SubresourceFilterSafeBrowsingActivationThrottle::ConfigResult::ConfigResult(
    Configuration config,
    bool warning,
    bool matched_valid_configuration,
    ActivationList matched_list)
    : config(config),
      warning(warning),
      matched_valid_configuration(matched_valid_configuration),
      matched_list(matched_list) {}

SubresourceFilterSafeBrowsingActivationThrottle::ConfigResult::ConfigResult() =
    default;

SubresourceFilterSafeBrowsingActivationThrottle::ConfigResult::ConfigResult(
    const ConfigResult&) = default;

SubresourceFilterSafeBrowsingActivationThrottle::ConfigResult::~ConfigResult() =
    default;

void SubresourceFilterSafeBrowsingActivationThrottle::CheckCurrentUrl() {
  DCHECK(database_client_);
  check_results_.emplace_back();
  size_t id = check_results_.size() - 1;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SubresourceFilterSafeBrowsingClient::CheckUrlOnIO,
                     base::Unretained(database_client_.get()),
                     navigation_handle()->GetURL(), id,
                     base::TimeTicks::Now()));
}

void SubresourceFilterSafeBrowsingActivationThrottle::NotifyResult() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SubresourceFilterSafeBrowsingActivationThrottle::NotifyResult");
  DCHECK(!check_results_.empty());

  // Determine which results to consider for safebrowsing/abusive enforcement.
  CheckResults check_results_to_consider = {check_results_.back()};
  if (check_results_.size() >= 2 &&
      base::FeatureList::IsEnabled(
          kSafeBrowsingSubresourceFilterConsiderRedirects)) {
    check_results_to_consider = {check_results_[0], check_results_.back()};
  }

  // Find the ConfigResult for each safe browsing check.
  std::vector<ConfigResult> matched_configurations;
  for (const auto& current_result : check_results_to_consider) {
    matched_configurations.push_back(
        GetHighestPriorityConfiguration(current_result));
  }

  // Get the activation decision with the associated ConfigResult.
  ConfigResult selection;
  ActivationDecision activation_decision =
      GetActivationDecision(matched_configurations, &selection);
  DCHECK_NE(activation_decision, ActivationDecision::UNKNOWN);

  // Notify the observers of the check results.
  SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifySafeBrowsingChecksComplete(navigation_handle(),
                                         check_results_to_consider);

  // Compute the activation level.
  mojom::ActivationLevel activation_level =
      selection.config.activation_options.activation_level;

  if (selection.warning &&
      activation_level == mojom::ActivationLevel::kEnabled) {
    NavigationConsoleLogger::LogMessageOnCommit(
        navigation_handle(), blink::mojom::ConsoleMessageLevel::kWarning,
        kActivationWarningConsoleMessage);
    activation_level = mojom::ActivationLevel::kDisabled;
  }

  // Let the embedder get the last word when it comes to activation level.
  // TODO(csharrison): Move all ActivationDecision code to the embedder.
  activation_level = client_->OnPageActivationComputed(
      navigation_handle(), activation_level, &activation_decision);

  LogMetricsOnChecksComplete(selection.matched_list, activation_decision,
                             activation_level);

  SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifyPageActivationComputed(
          navigation_handle(),
          selection.config.GetActivationState(activation_level));
}

void SubresourceFilterSafeBrowsingActivationThrottle::
    LogMetricsOnChecksComplete(ActivationList matched_list,
                               ActivationDecision decision,
                               mojom::ActivationLevel level) const {
  DCHECK(HasFinishedAllSafeBrowsingChecks());

  base::TimeDelta delay = defer_time_.is_null()
                              ? base::TimeDelta::FromMilliseconds(0)
                              : base::TimeTicks::Now() - defer_time_;
  UMA_HISTOGRAM_TIMES("SubresourceFilter.PageLoad.SafeBrowsingDelay", delay);

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::SubresourceFilter builder(source_id);
  builder.SetActivationDecision(static_cast<int64_t>(decision));
  if (level == mojom::ActivationLevel::kDryRun) {
    DCHECK_EQ(ActivationDecision::ACTIVATED, decision);
    builder.SetDryRun(true);
  }

  if (auto optional_position = GetEnforcementRedirectPosition(check_results_)) {
    RedirectPosition position = *optional_position;
    UMA_HISTOGRAM_ENUMERATION(
        "SubresourceFilter.PageLoad.Activation.RedirectPosition2.Enforcement",
        position);
    builder.SetEnforcementRedirectPosition(static_cast<int64_t>(position));
  }

  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationDecision",
                            decision,
                            ActivationDecision::ACTIVATION_DECISION_MAX);
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationList",
                            matched_list,
                            static_cast<int>(ActivationList::LAST) + 1);
}

bool SubresourceFilterSafeBrowsingActivationThrottle::
    HasFinishedAllSafeBrowsingChecks() const {
  for (const auto& check_result : check_results_) {
    if (!check_result.finished) {
      return false;
    }
  }
  return true;
}

SubresourceFilterSafeBrowsingActivationThrottle::ConfigResult
SubresourceFilterSafeBrowsingActivationThrottle::
    GetHighestPriorityConfiguration(
        const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  DCHECK(result.finished);
  Configuration selected_config;
  bool warning = false;
  bool matched = false;
  ActivationList matched_list = GetListForThreatTypeAndMetadata(
      result.threat_type, result.threat_metadata, &warning);
  // If it's http or https, find the best config.
  if (navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS()) {
    const auto& decreasing_configs =
        GetEnabledConfigurations()->configs_by_decreasing_priority();
    const auto selected_config_itr =
        std::find_if(decreasing_configs.begin(), decreasing_configs.end(),
                     [matched_list, this](const Configuration& config) {
                       return DoesMainFrameURLSatisfyActivationConditions(
                           config.activation_conditions, matched_list);
                     });
    if (selected_config_itr != decreasing_configs.end()) {
      selected_config = *selected_config_itr;
      matched = true;
    }
  }
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SubresourceFilterSafeBrowsingActivationThrottle::"
               "GetHighestPriorityConfiguration",
               "selected_config",
               !matched ? selected_config.ToTracedValue()
                        : std::make_unique<base::trace_event::TracedValue>());
  return ConfigResult(selected_config, warning, matched, matched_list);
}

ActivationDecision
SubresourceFilterSafeBrowsingActivationThrottle::GetActivationDecision(
    const std::vector<ConfigResult>& configs,
    ConfigResult* selected_config) {
  size_t selected_index = 0;
  for (size_t current_index = 0; current_index < configs.size();
       current_index++) {
    // Prefer later configs when there's a tie.
    // Rank no matching config slightly below priority zero.
    const int selected_priority =
        configs[selected_index].matched_valid_configuration
            ? configs[selected_index].config.activation_conditions.priority
            : -1;
    const int current_priority =
        configs[current_index].matched_valid_configuration
            ? configs[current_index].config.activation_conditions.priority
            : -1;
    if (current_priority >= selected_priority) {
      selected_index = current_index;
    }
  }
  // Ensure that the list was not empty, and assign the configuration.
  DCHECK(selected_index != configs.size());
  *selected_config = configs[selected_index];

  if (!selected_config->matched_valid_configuration) {
    return ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
  }

  // Get the activation level for the matching configuration.
  auto activation_level =
      selected_config->config.activation_options.activation_level;
  // Compute and return the activation decision.
  return activation_level == mojom::ActivationLevel::kDisabled
             ? ActivationDecision::ACTIVATION_DISABLED
             : ActivationDecision::ACTIVATED;
}

bool SubresourceFilterSafeBrowsingActivationThrottle::
    DoesMainFrameURLSatisfyActivationConditions(
        const Configuration::ActivationConditions& conditions,
        ActivationList matched_list) const {
  // Avoid copies when tracing disabled.
  auto list_to_string = [](ActivationList activation_list) {
    std::ostringstream matched_list_stream;
    matched_list_stream << activation_list;
    return matched_list_stream.str();
  };
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SubresourceFilterSafeBrowsingActivationThrottle::"
               "DoesMainFrameURLSatisfyActivationConditions",
               "matched_list", list_to_string(matched_list), "conditions",
               conditions.ToTracedValue());
  switch (conditions.activation_scope) {
    case ActivationScope::ALL_SITES:
      return true;
    case ActivationScope::ACTIVATION_LIST:
      if (matched_list == ActivationList::NONE)
        return false;
      if (conditions.activation_list == matched_list)
        return true;

      if (conditions.activation_list == ActivationList::PHISHING_INTERSTITIAL &&
          matched_list == ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL) {
        // Handling special case, where activation on the phishing sites also
        // mean the activation on the sites with social engineering metadata.
        return true;
      }
      if (conditions.activation_list == ActivationList::BETTER_ADS &&
          matched_list == ActivationList::ABUSIVE &&
          base::FeatureList::IsEnabled(kFilterAdsOnAbusiveSites)) {
        // Trigger activation on abusive sites if the condition says to trigger
        // on Better Ads sites. This removes the need for adding a separate
        // Configuration for Abusive enforcement.
        return true;
      }
      return false;
    case ActivationScope::NO_SITES:
      return false;
  }
  NOTREACHED();
  return false;
}

}  //  namespace subresource_filter
