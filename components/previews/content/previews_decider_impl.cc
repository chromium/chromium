// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_decider_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_switches.h"
#include "net/nqe/network_quality_estimator.h"

namespace previews {

namespace {

void LogPreviewsEligibilityReason(PreviewsEligibilityReason status,
                                  PreviewsType type) {
  int32_t max_limit = static_cast<int32_t>(PreviewsEligibilityReason::LAST);
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("Previews.EligibilityReason.%s",
                         GetStringNameForType(type).c_str()),
      1, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(status));
}

bool AllowedOnReload(PreviewsType type) {
  switch (type) {
    // These types return new content on refresh.
    case PreviewsType::LITE_PAGE:
    case PreviewsType::LITE_PAGE_REDIRECT:
    case PreviewsType::LOFI:
    case PreviewsType::NOSCRIPT:
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return true;
    // Loading these types will always be stale when refreshed.
    case PreviewsType::OFFLINE:
      return false;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

bool ShouldCheckOptimizationHints(PreviewsType type) {
  switch (type) {
    // These types may have server optimization hints.
    case PreviewsType::NOSCRIPT:
    case PreviewsType::RESOURCE_LOADING_HINTS:
    case PreviewsType::LITE_PAGE_REDIRECT:
      return true;
    // These types do not have server optimization hints.
    case PreviewsType::OFFLINE:
    case PreviewsType::LITE_PAGE:
    case PreviewsType::LOFI:
      return false;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

bool IsPreviewsBlacklistIgnoredViaFlag() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIgnorePreviewsBlacklist);
}

}  // namespace

PreviewsDeciderImpl::PreviewsDeciderImpl(
    base::Clock* clock)
    : blacklist_ignored_(IsPreviewsBlacklistIgnoredViaFlag()),
      clock_(clock),
      page_id_(1u),
      weak_factory_(this) {}

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

void PreviewsDeciderImpl::OnResourceLoadingHints(
    const GURL& document_gurl,
    const std::vector<std::string>& patterns_to_block) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_ui_service_->SetResourceLoadingHintsResourcePatternsToBlock(
      document_gurl, patterns_to_block);
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
    uint64_t page_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogPreviewsEligibilityReason(reason, type);
  previews_ui_service_->LogPreviewDecisionMade(
      reason, url, time, type, std::move(passed_reasons), page_id);
}

void PreviewsDeciderImpl::AddPreviewNavigation(const GURL& url,
                                               bool opt_out,
                                               PreviewsType type,
                                               uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time time =
      previews_black_list_->AddPreviewNavigation(url, opt_out, type);
  if (opt_out) {
    last_opt_out_time_ = time;
  }
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

bool PreviewsDeciderImpl::ShouldAllowPreview(PreviewsUserData* previews_data,
                                             const GURL& url,
                                             bool is_reload,
                                             PreviewsType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == PreviewsType::OFFLINE ||
         type == PreviewsType::LITE_PAGE_REDIRECT ||
         type == PreviewsType::NOSCRIPT ||
         type == PreviewsType::RESOURCE_LOADING_HINTS);
  // Consumers that need to specify a blacklist or ignore flag should use
  // ShouldAllowPreviewAtECT directly.
  return ShouldAllowPreviewAtECT(previews_data, url, is_reload, type,
                                 params::GetECTThresholdForPreview(type),
                                 std::vector<std::string>(), false);
}

bool PreviewsDeciderImpl::ShouldAllowPreviewAtECT(
    PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    PreviewsType type,
    net::EffectiveConnectionType effective_connection_type_threshold,
    const std::vector<std::string>& host_blacklist_from_finch,
    bool is_server_preview) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!previews::params::ArePreviewsAllowed()) {
    return false;
  }

  if (!url.has_host() || !previews_data) {
    // Don't capture UMA on this case, as it is not important and can happen
    // when navigating to files on disk, etc.
    return false;
  }

  std::vector<PreviewsEligibilityReason> passed_reasons;
  uint64_t page_id = previews_data->page_id();
  if (is_enabled_callback_.is_null() || !previews_black_list_) {
    LogPreviewDecisionMade(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
                           url, clock_->Now(), type, std::move(passed_reasons),
                           page_id);
    return false;
  }
  passed_reasons.push_back(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);

  if (!is_enabled_callback_.Run(type))
    return false;

  // In the case that the user has chosen to ignore the normal blacklist rules
  // (flags or interventions-internals), a preview should still not be served
  // for 5 seconds after the last opt out. This allows "show original" to
  // function correctly as the start of that navigation will be within 5 seconds
  // (we don't yet re-evaluate on redirects, so this is sufficient).
  if (blacklist_ignored_) {
    if (clock_->Now() < last_opt_out_time_ + base::TimeDelta::FromSeconds(5)) {
      LogPreviewDecisionMade(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
                             url, clock_->Now(), type,
                             std::move(passed_reasons), page_id);

      return false;
    }
  } else {
    // The blacklist will disallow certain hosts for periods of time based on
    // user's opting out of the preview.
    PreviewsEligibilityReason status = previews_black_list_->IsLoadedAndAllowed(
        url, type,
        is_server_preview && ignore_long_term_blacklist_for_server_previews_,
        &passed_reasons);

    if (status != PreviewsEligibilityReason::ALLOWED) {
      if (type == PreviewsType::LITE_PAGE) {
        previews_data->set_black_listed_for_lite_page(true);
      }
      LogPreviewDecisionMade(status, url, clock_->Now(), type,
                             std::move(passed_reasons), page_id);
      return false;
    }
  }

  if (effective_connection_type_threshold !=
      net::EFFECTIVE_CONNECTION_TYPE_LAST) {
    // Network quality estimator may sometimes return effective connection type
    // as offline when the Android APIs incorrectly return device connectivity
    // as null. See https://crbug.com/838969. So, we do not trigger previews
    // when |observed_effective_connection_type| is
    // net::EFFECTIVE_CONNECTION_TYPE_OFFLINE.
    if (effective_connection_type_ <= net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
      LogPreviewDecisionMade(
          PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE, url,
          clock_->Now(), type, std::move(passed_reasons), page_id);
      return false;
    }
    passed_reasons.push_back(
        PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE);

    if (effective_connection_type_ > effective_connection_type_threshold) {
      LogPreviewDecisionMade(PreviewsEligibilityReason::NETWORK_NOT_SLOW, url,
                             clock_->Now(), type, std::move(passed_reasons),
                             page_id);
      return false;
    }
    passed_reasons.push_back(PreviewsEligibilityReason::NETWORK_NOT_SLOW);
  }

  // LOAD_VALIDATE_CACHE or LOAD_BYPASS_CACHE mean the user reloaded the page.
  // If this is a query for offline previews, reloads should be disallowed.
  if (!AllowedOnReload(type) && is_reload) {
    LogPreviewDecisionMade(PreviewsEligibilityReason::RELOAD_DISALLOWED, url,
                           clock_->Now(), type, std::move(passed_reasons),
                           page_id);
    return false;
  }
  passed_reasons.push_back(PreviewsEligibilityReason::RELOAD_DISALLOWED);

  // Check Finch-provided blacklist, if any. This type of blacklist was added
  // for Finch provided blacklist for Client LoFi.
  if (base::ContainsValue(host_blacklist_from_finch, url.host_piece())) {
    LogPreviewDecisionMade(
        PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER, url,
        clock_->Now(), type, std::move(passed_reasons), page_id);
    return false;
  }
  passed_reasons.push_back(
      PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER);

  // Check server whitelist/blacklist, if provided.
  if (ShouldCheckOptimizationHints(type)) {
    if (params::IsOptimizationHintsEnabled()) {
      // Optimization hints are configured, so determine if those hints
      // allow the optimization type (as of start-of-navigation time anyway).
      PreviewsEligibilityReason status = ShouldAllowPreviewPerOptimizationHints(
          previews_data, url, type, &passed_reasons);
      if (status != PreviewsEligibilityReason::ALLOWED) {
        LogPreviewDecisionMade(status, url, clock_->Now(), type,
                               std::move(passed_reasons), page_id);
        return false;
      }
    } else if (type == PreviewsType::RESOURCE_LOADING_HINTS) {
      // RESOURCE_LOADING_HINTS optimization can be applied only when a server
      // provided whitelist is available.
      LogPreviewDecisionMade(
          PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER, url,
          clock_->Now(), type, std::move(passed_reasons), page_id);
      return false;
    } else {
      DCHECK(type == PreviewsType::LITE_PAGE_REDIRECT ||
             type == PreviewsType::NOSCRIPT);
      // Since server optimization guidance not configured, allow the preview
      // but with qualified eligibility reason.
      LogPreviewDecisionMade(
          PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS, url,
          clock_->Now(), type, std::move(passed_reasons), page_id);
      return true;
    }
  }

  LogPreviewDecisionMade(PreviewsEligibilityReason::ALLOWED, url, clock_->Now(),
                         type, std::move(passed_reasons), page_id);
  return true;
}

void PreviewsDeciderImpl::LoadResourceHints(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_opt_guide_->MaybeLoadOptimizationHints(
      url, base::BindOnce(&PreviewsDeciderImpl::OnResourceLoadingHints,
                          weak_factory_.GetWeakPtr()));
}

void PreviewsDeciderImpl::LogHintCacheMatch(const GURL& url,
                                            bool is_committed) const {
  if (!previews_opt_guide_)
    return;

  previews_opt_guide_->LogHintCacheMatch(url, is_committed,
                                         effective_connection_type_);
}

bool PreviewsDeciderImpl::IsURLAllowedForPreview(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PreviewsType::NOSCRIPT == type ||
         PreviewsType::RESOURCE_LOADING_HINTS == type);
  if (previews_black_list_ && !blacklist_ignored_) {
    std::vector<PreviewsEligibilityReason> passed_reasons;
    // The blacklist will disallow certain hosts for periods of time based on
    // user's opting out of the preview.
    PreviewsEligibilityReason status = previews_black_list_->IsLoadedAndAllowed(
        url, type, false, &passed_reasons);
    if (status != PreviewsEligibilityReason::ALLOWED) {
      LogPreviewDecisionMade(status, url, clock_->Now(), type,
                             std::move(passed_reasons),
                             previews_data->page_id());
      return false;
    }
  }

  // Re-check server optimization hints (if provided) on this commit-time URL.
  if (ShouldCheckOptimizationHints(type)) {
    if (params::IsOptimizationHintsEnabled()) {
      std::vector<PreviewsEligibilityReason> passed_reasons;
      PreviewsEligibilityReason status =
          IsURLAllowedForPreviewByOptimizationHints(previews_data, url, type,
                                                    &passed_reasons);
      if (status != PreviewsEligibilityReason::ALLOWED) {
        LogPreviewDecisionMade(status, url, clock_->Now(), type,
                               std::move(passed_reasons),
                               previews_data->page_id());
        return false;
      }
    }
  }
  return true;
}

PreviewsEligibilityReason
PreviewsDeciderImpl::ShouldAllowPreviewPerOptimizationHints(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == PreviewsType::LITE_PAGE_REDIRECT ||
         type == PreviewsType::NOSCRIPT ||
         type == PreviewsType::RESOURCE_LOADING_HINTS);

  // Per-PreviewsType default if no optimization guide data.
  if (!previews_opt_guide_) {
    if (type == PreviewsType::NOSCRIPT) {
      return PreviewsEligibilityReason::ALLOWED;
    } else {
      return PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER;
    }
  }

  // For LitePageRedirect, ensure it is not blacklisted for this request.
  if (type == PreviewsType::LITE_PAGE_REDIRECT) {
    if (previews_opt_guide_->IsBlacklisted(url, type)) {
      return PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER;
    }
    passed_reasons->push_back(
        PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER);
  }

  // For NoScript, ensure it is whitelisted for this request.
  if (type == PreviewsType::NOSCRIPT) {
    if (!previews_opt_guide_->IsWhitelisted(previews_data, url, type)) {
      return PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER;
    }
    passed_reasons->push_back(
        PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER);
  }

  // Note: allow ResourceLoadingHints since the guide is available. Hints may
  // need to be loaded from it for commit time detail check.

  return PreviewsEligibilityReason::ALLOWED;
}

PreviewsEligibilityReason
PreviewsDeciderImpl::IsURLAllowedForPreviewByOptimizationHints(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type == PreviewsType::LITE_PAGE_REDIRECT ||
         type == PreviewsType::NOSCRIPT ||
         type == PreviewsType::RESOURCE_LOADING_HINTS);

  // For NoScript, if optimization guide is not present, assume that all URLs
  // are ALLOWED.
  if (!previews_opt_guide_ && type == PreviewsType::NOSCRIPT)
    return PreviewsEligibilityReason::ALLOWED;

  // Check if request URL is whitelisted by the optimization guide.
  if (!previews_opt_guide_->IsWhitelisted(previews_data, url, type)) {
    return PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER;
  }
  passed_reasons->push_back(
      PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER);

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
}  // namespace previews
