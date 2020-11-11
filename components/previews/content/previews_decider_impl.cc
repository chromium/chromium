// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_decider_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
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

}  // namespace

PreviewsDeciderImpl::PreviewsDeciderImpl(base::Clock* clock)
    : blocklist_ignored_(switches::ShouldIgnorePreviewsBlocklist()),
      clock_(clock),
      page_id_(1u) {}

PreviewsDeciderImpl::~PreviewsDeciderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PreviewsDeciderImpl::Initialize(
    PreviewsUIService* previews_ui_service,
    std::unique_ptr<blocklist::OptOutStore> previews_opt_out_store,
    std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
    const PreviewsIsEnabledCallback& is_enabled_callback,
    blocklist::BlocklistData::AllowedTypesAndVersions allowed_previews) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_enabled_callback.is_null());
  is_enabled_callback_ = is_enabled_callback;
  previews_ui_service_ = previews_ui_service;
  previews_opt_guide_ = std::move(previews_opt_guide);

  previews_block_list_ = std::make_unique<PreviewsBlockList>(
      std::move(previews_opt_out_store), clock_, this,
      std::move(allowed_previews));
}

void PreviewsDeciderImpl::OnNewBlocklistedHost(const std::string& host,
                                               base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnNewBlocklistedHost(host, time);
}

void PreviewsDeciderImpl::OnUserBlocklistedStatusChange(bool blocklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnUserBlocklistedStatusChange(blocklisted);
}

void PreviewsDeciderImpl::OnBlocklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->OnBlocklistCleared(time);
}

void PreviewsDeciderImpl::SetPreviewsBlocklistForTesting(
    std::unique_ptr<PreviewsBlockList> previews_block_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_block_list_ = std::move(previews_block_list);
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
      previews_block_list_->AddPreviewNavigation(url, opt_out, type);
  LogPreviewNavigation(url, opt_out, type, time, page_id);
}

void PreviewsDeciderImpl::ClearBlockList(base::Time begin_time,
                                         base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  previews_block_list_->ClearBlockList(begin_time, end_time);
}

void PreviewsDeciderImpl::SetIgnorePreviewsBlocklistDecision(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocklist_ignored_ = ignored;
  previews_ui_service_->OnIgnoreBlocklistDecisionStatusChanged(
      blocklist_ignored_);
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

  std::vector<PreviewsEligibilityReason> passed_reasons;
  PreviewsEligibilityReason eligibility = DeterminePreviewEligibility(
      previews_data, navigation_handle, is_reload, type, &passed_reasons);
  LogPreviewDecisionMade(eligibility, url, clock_->Now(), type,
                         std::move(passed_reasons), previews_data);
  if (previews_opt_guide_ &&
      eligibility == PreviewsEligibilityReason::ALLOWED) {
    // Kick off model prediction.
    previews_opt_guide_->StartCheckingIfShouldShowPreview(navigation_handle);
  }
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

  // TODO(sophiechang): Remove the ECT unknown and offline checks when
  // optimization guide checks for those values specifically.

  if (is_reload) {
    return PreviewsEligibilityReason::RELOAD_DISALLOWED;
  }
  passed_reasons->push_back(PreviewsEligibilityReason::RELOAD_DISALLOWED);

  bool skip_hint_check =
      (type == PreviewsType::DEFER_ALL_SCRIPT &&
       base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableDeferAllScriptWithoutOptimizationHints));

  // Check optimization hints, if provided.
  if (!skip_hint_check) {
    if (previews_opt_guide_) {
      // Optimization hints are configured, so determine if those hints
      // allow the optimization type (as of start-of-navigation time anyway).
      return PreviewsEligibilityReason::ALLOWED;
    } else {
      return PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE;
    }
  }

  return PreviewsEligibilityReason::ALLOWED;
}

PreviewsEligibilityReason PreviewsDeciderImpl::CheckLocalBlocklist(
    const GURL& url,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  if (!previews_block_list_)
    return PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE;
  passed_reasons->push_back(PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE);

  // Trigger the USER_RECENTLY_OPTED_OUT rule when a reload on a preview has
  // occurred recently. No need to push_back the eligibility reason as it will
  // be added in IsLoadedAndAllowed as the first check.
  if (recent_preview_reload_time_ &&
      recent_preview_reload_time_.value() + params::SingleOptOutDuration() >
          clock_->Now()) {
    return PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT;
  }

  // The blocklist will disallow certain hosts for periods of time based on
  // user's opting out of the preview.
  return previews_block_list_->IsLoadedAndAllowed(url, type, passed_reasons);
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

  // Check optimization hints (if provided) on this commit-time URL.
  if (previews_opt_guide_) {
    std::vector<PreviewsEligibilityReason> passed_reasons;
    PreviewsEligibilityReason status = ShouldCommitPreviewPerOptimizationHints(
        previews_data, navigation_handle, type, &passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      LogPreviewDecisionMade(status, committed_url, clock_->Now(), type,
                             std::move(passed_reasons), previews_data);
      return false;
    }
  }

  // Check local blocklist for commit-time preview (if blocklist not ignored).
  if (!blocklist_ignored_) {
    std::vector<PreviewsEligibilityReason> passed_reasons;
    PreviewsEligibilityReason status =
        CheckLocalBlocklist(committed_url, type, &passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      LogPreviewDecisionMade(status, committed_url, clock_->Now(), type,
                             std::move(passed_reasons), previews_data);
      return false;
    }
  }

  return true;
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

  // Check if the page load is predicted to be painful.
  if (!previews_opt_guide_->ShouldShowPreview(navigation_handle))
    return PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL;
  passed_reasons->push_back(
      PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL);

  // Check if request URL is allowlisted by the optimization guide.
  if (!previews_opt_guide_->CanApplyPreview(previews_data, navigation_handle,
                                            type)) {
    return PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE);

  // TODO(sophiechang): Remove below ECT unknown and offline checks when
  // optimization guide checks for those values specifically.

  // Note: the network quality estimator may sometimes return effective
  // connection type as offline when the Android APIs incorrectly return device
  // connectivity as null. See https://crbug.com/838969. So, we do not trigger
  // previews when |ect| is net::EFFECTIVE_CONNECTION_TYPE_OFFLINE.
  net::EffectiveConnectionType ect = previews_data->navigation_ect();
  if (ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    // Update the |ect| to the current value.
    ect = effective_connection_type_;
  }

  if (ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE);

  if (ect == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    return PreviewsEligibilityReason::DEVICE_OFFLINE;
  }
  passed_reasons->push_back(PreviewsEligibilityReason::DEVICE_OFFLINE);

  return PreviewsEligibilityReason::ALLOWED;
}

uint64_t PreviewsDeciderImpl::GeneratePageId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
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
