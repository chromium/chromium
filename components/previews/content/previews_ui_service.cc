// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_ui_service.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "url/gurl.h"

namespace previews {

PreviewsUIService::PreviewsUIService(
    std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
    std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
    std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
    const PreviewsIsEnabledCallback& is_enabled_callback,
    std::unique_ptr<PreviewsLogger> logger,
    blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews,
    network::NetworkQualityTracker* network_quality_tracker)
    : previews_decider_impl_(std::move(previews_decider_impl)),
      logger_(std::move(logger)),
      network_quality_tracker_(network_quality_tracker) {
  DCHECK(logger_);
  DCHECK(previews_decider_impl_);
  DCHECK(network_quality_tracker_);
  previews_decider_impl_->Initialize(
      this, std::move(previews_opt_out_store), std::move(previews_opt_guide),
      is_enabled_callback, std::move(allowed_previews));
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
}

PreviewsUIService::~PreviewsUIService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
}

void PreviewsUIService::AddPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->AddPreviewNavigation(url, opt_out, type, page_id);
}

void PreviewsUIService::LogPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             base::Time time,
                                             uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogPreviewNavigation(url, type, opt_out, time, page_id);
}

void PreviewsUIService::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons,
    uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogPreviewDecisionMade(reason, url, time, type,
                                  std::move(passed_reasons), page_id);
}

void PreviewsUIService::OnNewBlacklistedHost(const std::string& host,
                                             base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnNewBlacklistedHost(host, time);
}

void PreviewsUIService::OnUserBlacklistedStatusChange(bool blacklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnUserBlacklistedStatusChange(blacklisted);
}

void PreviewsUIService::OnBlacklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnBlacklistCleared(time);
}

void PreviewsUIService::SetIgnorePreviewsBlacklistDecision(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->SetIgnorePreviewsBlacklistDecision(ignored);
}

void PreviewsUIService::OnIgnoreBlacklistDecisionStatusChanged(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnIgnoreBlacklistDecisionStatusChanged(ignored);
}

std::vector<std::string>
PreviewsUIService::GetResourceLoadingHintsResourcePatternsToBlock(
    const GURL& document_gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> resource_patterns_to_block;
  if (previews_decider_impl_) {
    previews_decider_impl_->GetResourceLoadingHints(
        document_gurl, &resource_patterns_to_block);
  }
  return resource_patterns_to_block;
}

PreviewsLogger* PreviewsUIService::previews_logger() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return logger_.get();
}

PreviewsDeciderImpl* PreviewsUIService::previews_decider_impl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return previews_decider_impl_.get();
}

// When triggering previews, prevent long term black list rules.
void PreviewsUIService::SetIgnoreLongTermBlackListForServerPreviews(
    bool ignore_long_term_black_list_rules_allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  previews_decider_impl_->SetIgnoreLongTermBlackListForServerPreviews(
      ignore_long_term_black_list_rules_allowed);
}

void PreviewsUIService::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->ClearBlackList(begin_time, end_time);
}

void PreviewsUIService::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_effective_connection_type_ = type;
  previews_decider_impl_->SetEffectiveConnectionType(type);
}

}  // namespace previews
