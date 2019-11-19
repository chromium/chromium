// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_decider_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "net/nqe/network_quality_estimator.h"

namespace previews {

namespace {

void LogPreviewsEligibilityReason(PreviewsEligibilityReason status,
                                  PreviewsType type) {
  DCHECK_LT(status, PreviewsEligibilityReason::LAST);
  UMA_HISTOGRAM_ENUMERATION("Previews.EligibilityReason", status,
                            PreviewsEligibilityReason::LAST);
  int32_t max_limit = static_cast<int32_t>(PreviewsEligibilityReason::LAST);
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("Previews.EligibilityReason.%s",
                         GetStringNameForType(type).c_str()),
      1, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(status));
}

bool ShouldCheckOptimizationHints(PreviewsType type) {
  switch (type) {
    // These types may have server optimization hints.
    case PreviewsType::NOSCRIPT:
    case PreviewsType::RESOURCE_LOADING_HINTS:
    case PreviewsType::LITE_PAGE_REDIRECT:
    case PreviewsType::DEFER_ALL_SCRIPT:
      return true;
    // These types do not have server optimization hints.
    case PreviewsType::OFFLINE:
    case PreviewsType::LITE_PAGE:
      return false;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Returns true if the decision to apply |type| can wait until commit time.
bool IsCommitTimePreview(PreviewsType type) {
  switch (type) {
    case PreviewsType::NOSCRIPT:
    case PreviewsType::RESOURCE_LOADING_HINTS:
    case PreviewsType::DEFER_ALL_SCRIPT:
      return true;
    case PreviewsType::LITE_PAGE_REDIRECT:
    case PreviewsType::OFFLINE:
    case PreviewsType::LITE_PAGE:
      return false;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// We don't care if the ECT is unknown if the slow page threshold is set to 4G
// (i.e.: all pages).
bool ShouldCheckForUnknownECT(net::EffectiveConnectionType ect) {
  if (!base::FeatureList::IsEnabled(features::kSlowPageTriggering))
    return true;

  return ect != net::EFFECTIVE_CONNECTION_TYPE_LAST - 1;
}

}  // namespace

PreviewsDeciderImpl::PreviewsDeciderImpl(base::Clock* clock)
    : blacklist_ignored_(switches::ShouldIgnorePreviewsBlacklist()),
      clock_(clock),
      page_id_(1u) {}

PreviewsDeciderImpl::~PreviewsDeciderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PreviewsDeciderImpl::Initialize(
    PreviewsUIService* previews_ui_service,
    std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
    std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
    const PreviewsIsEnabledCallback& is_enabled_callback,
    blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_enabled_callback.is_null());
  is_enabled_callback_ = is_enabled_callback;
  previews_ui_service_ = previews_ui_service;
  previews_opt_guide_ = std::move(previews_opt_guide);

  previews_black_list_ = std::make_unique<PreviewsBlackList>(
      std::move(previews_opt_out_store), clock_, this,
      std::move(allowed_previews));
}

void PreviewsDeciderImpl::OnNewBlacklistedHost(const std::string& host,
                                               base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnNewBlacklistedHost(host, time);
}

void PreviewsDeciderImpl::OnUserBlacklistedStatusChange(bool blacklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnUserBlacklistedStatusChange(blacklisted);
}

void PreviewsDeciderImpl::OnBlacklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnBlacklistCleared(time);
}

void PreviewsDeciderImpl::SetPreviewsBlacklistForTesting(
    std::unique_ptr<PreviewsBlackList> previews_back_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_black_list_ = std::move(previews_back_list);
}

void PreviewsDeciderImpl::LogPreviewNavigation(const GURL& url,
                                               bool opt_out,
                                               PreviewsType type,
                                               base::Time time,
                                               uint64_t page_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->LogPreviewNavigation(url, type, opt_out, time, page_id);
}

void PreviewsDeciderImpl::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons,
    PreviewsUserData* user_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogPreviewsEligibilityReason(reason, type);
  user_data->SetEligibilityReasonForPreview(type, reason);
  previews_ui_service_->LogPreviewDecisionMade(
      reason, url, time, type, std::move(passed_reasons), user_data->page_id());
}

void PreviewsDeciderImpl::AddPreviewNavigation(const GURL& url,
                                               bool opt_out,
                                               PreviewsType type,
                                               uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time time =
      previews_black_list_->AddPreviewNavigation(url, opt_out, type);
  LogPreviewNavigation(url, opt_out, type, time, page_id);
}

void PreviewsDeciderImpl::ClearBlackList(base::Time begin_time,
                                         base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  previews_black_list_->ClearBlackList(begin_time, end_time);
}

void PreviewsDeciderImpl::SetIgnorePreviewsBlacklistDecision(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklist_ignored_ = ignored;
  previews_ui_service_->OnIgnoreBlacklistDecisionStatusChanged(
      blacklist_ignored_);
}

bool PreviewsDeciderImpl::ShouldAllowPreviewAtNavigationStart(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    bool is_reload,
    PreviewsType type) const {
  const GURL url = navigation_handle->GetURL();

  if (!ShouldConsiderPreview(type, url, previews_data)) {
    // Don't capture metrics since preview is either disabled or url is local.
    return false;
  }

  bool is_drp_server_preview = (type == PreviewsType::LITE_PAGE);
  std::vector<PreviewsEligibilityReason> passed_reasons;
  PreviewsEligibilityReason eligibility =
      DeterminePreviewEligibility(previews_data, navigation_handle, is_reload,
                                  type, is_drp_server_preview, &passed_reasons);
  LogPreviewDecisionMade(eligibility, url, clock_->Now(), type,
                         std::move(passed_reasons), previews_data);
  return eligibility == PreviewsEligibilityReason::ALLOWED;
}

bool PreviewsDeciderImpl::ShouldConsiderPreview(
    PreviewsType type,
    const GURL& url,
    PreviewsUserData* previews_data) const {
  return previews::params::ArePreviewsAllowed() &&
         is_enabled_callback_.Run(type) && url.has_host() &&
         url.SchemeIsHTTPOrHTTPS() && previews_data;
}

PreviewsEligibilityReason PreviewsDeciderImpl::DeterminePreviewEligibility(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    bool is_reload,
    PreviewsType type,
    bool is_drp_server_preview,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(previews::params::ArePreviewsAllowed());
  const GURL url = navigation_handle->GetURL();
  DCHECK(url.has_host());
  DCHECK(previews_data);

  // Capture the effective connection type at this time of determining
  // eligibility so that it will be available at commit time.
  previews_data->set_navigation_ect(effective_connection_type_);

  // Do not allow previews on any authenticated pages.
  if (url.has_username() || url.has_password())
    return PreviewsEligibilityReason::URL_HAS_BASIC_AUTH;
  passed_reasons->push_back(PreviewsEligibilityReason::URL_HAS_BASIC_AUTH);

  // Do not allow previews for URL suffixes which are excluded. In practice,
  // this is used to exclude navigations that look like media resources like
  // navigating to http://chromium.org/video.mp4.
  if (params::ShouldExcludeMediaSuffix(url))
    return PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX;
  passed_reasons->push_back(
      PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX);

  // Check the network quality for client previews that don't have optimization
  // hints. This defers checking ECT for server previews because the server will
  // perform its own ECT check and for previews with hints because the hints may
  // specify variable ECT thresholds for slow page hints.
  if (!is_drp_server_preview && !IsCommitTimePreview(type)) {
    if (effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
      return PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE;
    }
    passed_reasons->push_back(
        PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE);

    // Network quality estimator may sometimes return effective connection type
    // as offline when the Android APIs incorrectly return device connectivity
    // as null. See https://crbug.com/838969. So, we do not trigger previews
    // when |observed_effective_connection_type| is
    // net::EFFECTIVE_CONNECTION_TYPE_OFFLINE.
    if (effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
      return PreviewsEligibilityReason::DEVICE_OFFLINE;
    }
    passed_reasons->push_back(PreviewsEligibilityReason::DEVICE_OFFLINE);

    // If the optimization type is not a commit-time preview, determine
    // the ECT network triggering condition here.
    if (!IsCommitTimePreview(type)) {
      if (effective_connection_type_ >
          previews::params::GetECTThresholdForPreview(type)) {
        return PreviewsEligibilityReason::NETWORK_NOT_SLOW;
      }
      passed_reasons->push_back(PreviewsEligibilityReason::NETWORK_NOT_SLOW);

      if (effective_connection_type_ > params::GetSessionMaxECTThreshold()) {
        return PreviewsEligibilityReason::NETWORK_NOT_SLOW_FOR_SESSION;
      }
      passed_reasons->push_back(
          PreviewsEligibilityReason::NETWORK_NOT_SLOW_FOR_SESSION);
    }
  }

  if (is_reload) {
    return PreviewsEligibilityReason::RELOAD_DISALLOWED;
  }
  passed_reasons->push_back(PreviewsEligibilityReason::RELOAD_DISALLOWED);

  // Check optimization hints, if provided.
  if (ShouldCheckOptimizationHints(type)) {
    if (previews_opt_guide_) {
      // Optimization hints are configured, so determine if those hints
      // allow the optimization type (as of start-of-navigation time anyway).
      return ShouldAllowPreviewPerOptimizationHints(
          previews_data, navigation_handle, type, passed_reasons);
    } else if (type == PreviewsType::RESOURCE_LOADING_HINTS ||
               type == PreviewsType::NOSCRIPT ||
               type == PreviewsType::DEFER_ALL_SCRIPT) {
      return PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE;
    }
  }

  // Skip blacklist checks if the blacklist is ignored or defer check until
  // commit time if preview type is to be decided at commit time.
  if (!blacklist_ignored_ && !IsCommitTimePreview(type)) {
    PreviewsEligibilityReason status =
        CheckLocalBlacklist(url, type, is_drp_server_preview, passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      if (type == PreviewsType::LITE_PAGE) {
        previews_data->set_black_listed_for_lite_page(true);
      }
      return status;
    }
  }

  return PreviewsEligibilityReason::ALLOWED;
}

PreviewsEligibilityReason PreviewsDeciderImpl::CheckLocalBlacklist(
    const GURL& url,
    PreviewsType type,
    bool is_drp_server_preview,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  if (!previews_black_list_)
    return PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  passed_reasons->push_back(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);

  // Trigger the USER_RECENTLY_OPTED_OUT rule when a reload on a preview has
  // occurred recently. No need to push_back the eligibility reason as it will
  // be added in IsLoadedAndAllowed as the first check.
  if (recent_preview_reload_time_ &&
      recent_preview_reload_time_.value() + params::SingleOptOutDuration() >
          clock_->Now()) {
    return PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT;
  }

  // The blacklist will disallow certain hosts for periods of time based on
  // user's opting out of the preview.
  return previews_black_list_->IsLoadedAndAllowed(
      url, type,
      is_drp_server_preview && ignore_long_term_blacklist_for_server_previews_,
      passed_reasons);
}

bool PreviewsDeciderImpl::LoadPageHints(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!previews_opt_guide_)
    return false;

  return previews_opt_guide_->MaybeLoadOptimizationHints(navigation_handle,
                                                         base::DoNothing());
}

bool PreviewsDeciderImpl::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!previews_opt_guide_)
    return false;

  return previews_opt_guide_->GetResourceLoadingHints(
      url, out_resource_patterns_to_block);
}

bool PreviewsDeciderImpl::ShouldCommitPreview(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    PreviewsType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PreviewsType::NOSCRIPT == type ||
         PreviewsType::RESOURCE_LOADING_HINTS == type ||
         PreviewsType::DEFER_ALL_SCRIPT == type);

  const GURL committed_url = navigation_handle->GetURL();

  // Re-check server optimization hints (if provided) on this commit-time URL.
  if (ShouldCheckOptimizationHints(type) && previews_opt_guide_) {
    std::vector<PreviewsEligibilityReason> passed_reasons;
    PreviewsEligibilityReason status = ShouldCommitPreviewPerOptimizationHints(
        previews_data, navigation_handle, type, &passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      LogPreviewDecisionMade(status, committed_url, clock_->Now(), type,
                             std::move(passed_reasons), previews_data);
      return false;
    }
  }

  // Check local blacklist for commit-time preview (if blacklist not ignored).
  if (!blacklist_ignored_ && IsCommitTimePreview(type)) {
    std::vector<PreviewsEligibilityReason> passed_reasons;
    PreviewsEligibilityReason status =
        CheckLocalBlacklist(committed_url, type, false, &passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      LogPreviewDecisionMade(status, committed_url, clock_->Now(), type,
                             std::move(passed_reasons), previews_data);
      return false;
    }
  }

  return true;
}

PreviewsEligibilityReason
PreviewsDeciderImpl::ShouldAllowPreviewPerOptimizationHints(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == PreviewsType::LITE_PAGE_REDIRECT ||
         type == PreviewsType::NOSCRIPT ||
         type == PreviewsType::RESOURCE_LOADING_HINTS ||
         type == PreviewsType::DEFER_ALL_SCRIPT);
  // For LitePageRedirect, ensure it is not blacklisted for this request, and
  // hints have been fully loaded.
  //
  // We allow all other Optimization Hint previews in the hopes that the missing
  // state will load in before commit.
  if (type == PreviewsType::LITE_PAGE_REDIRECT) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kIgnoreLitePageRedirectOptimizationBlacklist)) {
      // Make sure to also check the ECT threshold for the Preview if we are
      // bypassing the optimization guide.
      if (effective_connection_type_ >
          params::GetECTThresholdForPreview(type)) {
        return PreviewsEligibilityReason::NETWORK_NOT_SLOW;
      }
      return PreviewsEligibilityReason::ALLOWED;
    }

    if (!previews_opt_guide_)
      return PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE;
    passed_reasons->push_back(
        PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE);

    if (!previews_opt_guide_->CanApplyPreview(previews_data, navigation_handle,
                                              type)) {
      return PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE;
    }
    passed_reasons->push_back(
        PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE);
  }

  return PreviewsEligibilityReason::ALLOWED;
}

PreviewsEligibilityReason
PreviewsDeciderImpl::ShouldCommitPreviewPerOptimizationHints(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == PreviewsType::NOSCRIPT ||
         type == PreviewsType::RESOURCE_LOADING_HINTS ||
         type == PreviewsType::DEFER_ALL_SCRIPT);

  // If kEnableDeferAllScriptWithoutOptimizationHints switch is provided, then
  // DEFER_ALL_SCRIPT is triggered on all pages irrespective of hints provided
  // by optimization hints guide.
  if (type == PreviewsType::DEFER_ALL_SCRIPT &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDeferAllScriptWithoutOptimizationHints)) {
    return PreviewsEligibilityReason::ALLOWED;
  }

  if (!previews_opt_guide_)
    return PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE;
  passed_reasons->push_back(
      PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE);

  // Check if request URL is whitelisted by the optimization guide.
  if (!previews_opt_guide_->CanApplyPreview(previews_data, navigation_handle,
                                            type)) {
    return PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE);

  // The url is whitelisted, now check some additional cases of the effective
  // network condition.

  // Note: the network quality estimator may sometimes return effective
  // connection type as offline when the Android APIs incorrectly return device
  // connectivity as null. See https://crbug.com/838969. So, we do not trigger
  // previews when |ect| is net::EFFECTIVE_CONNECTION_TYPE_OFFLINE.
  net::EffectiveConnectionType ect = previews_data->navigation_ect();
  if (IsCommitTimePreview(type) &&
      ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    // Update the |ect| to the current value.
    ect = effective_connection_type_;
  }

  if (ShouldCheckForUnknownECT(params::GetSessionMaxECTThreshold()) &&
      ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE);

  if (ect == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    return PreviewsEligibilityReason::DEVICE_OFFLINE;
  }
  passed_reasons->push_back(PreviewsEligibilityReason::DEVICE_OFFLINE);

  if (ect > params::GetSessionMaxECTThreshold()) {
    return PreviewsEligibilityReason::NETWORK_NOT_SLOW_FOR_SESSION;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::NETWORK_NOT_SLOW_FOR_SESSION);
  return PreviewsEligibilityReason::ALLOWED;
}

uint64_t PreviewsDeciderImpl::GeneratePageId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
}

void PreviewsDeciderImpl::SetIgnoreLongTermBlackListForServerPreviews(
    bool ignore_long_term_blacklist_for_server_previews) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ignore_long_term_blacklist_for_server_previews_ =
      ignore_long_term_blacklist_for_server_previews;
}

void PreviewsDeciderImpl::SetEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_ = effective_connection_type;
}

void PreviewsDeciderImpl::AddPreviewReload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recent_preview_reload_time_ = clock_->Now();
}

}  // namespace previews
