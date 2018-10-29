// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_user_data.h"
#include "url/gurl.h"

namespace previews {

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : optimization_guide_service_(optimization_guide_service),
      io_task_runner_(io_task_runner),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      io_weak_ptr_factory_(this) {
  DCHECK(optimization_guide_service_);
  optimization_guide_service_->AddObserver(this);
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() {
  optimization_guide_service_->RemoveObserver(this);
}

bool PreviewsOptimizationGuide::IsWhitelisted(PreviewsUserData* previews_data,
                                              const GURL& url,
                                              PreviewsType type) const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!hints_)
    return false;

  int inflation_percent = 0;
  net::EffectiveConnectionType ect_threshold =
      params::GetECTThresholdForPreview(type);
  if (!hints_->IsWhitelisted(url, type, &inflation_percent, &ect_threshold))
    return false;

  if (inflation_percent != 0 && previews_data)
    previews_data->SetDataSavingsInflationPercent(inflation_percent);

  return true;
}

bool PreviewsOptimizationGuide::IsBlacklisted(const GURL& url,
                                              PreviewsType type) const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!hints_)
    return false;

  return hints_->IsBlacklisted(url, type);
}

void PreviewsOptimizationGuide::OnLoadedHint(
    ResourceLoadingHintsCallback callback,
    const GURL& document_url,
    const optimization_guide::proto::Hint& loaded_hint) const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  const optimization_guide::proto::PageHint* matched_page_hint =
      PreviewsHints::FindPageHint(document_url, loaded_hint);
  if (!matched_page_hint)
    return;

  // Retrieve the resource patterns to be blocked from the page hint.
  std::vector<std::string> resource_patterns_to_block;
  for (const auto& optimization :
       matched_page_hint->whitelisted_optimizations()) {
    if (optimization.optimization_type() ==
        optimization_guide::proto::RESOURCE_LOADING) {
      for (const auto& resource_loading_hint :
           optimization.resource_loading_hints()) {
        if (!resource_loading_hint.resource_pattern().empty() &&
            resource_loading_hint.loading_optimization_type() ==
                optimization_guide::proto::LOADING_BLOCK_RESOURCE) {
          resource_patterns_to_block.push_back(
              resource_loading_hint.resource_pattern());
        }
      }
    }
  }
  if (!resource_patterns_to_block.empty()) {
    std::move(callback).Run(document_url, resource_patterns_to_block);
  }
}

bool PreviewsOptimizationGuide::MaybeLoadOptimizationHints(
    const GURL& url,
    ResourceLoadingHintsCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!hints_)
    return false;

  return hints_->MaybeLoadOptimizationHints(
      url, base::BindOnce(&PreviewsOptimizationGuide::OnLoadedHint,
                          io_weak_ptr_factory_.GetWeakPtr(),
                          std::move(callback), url));
}

void PreviewsOptimizationGuide::LogHintCacheMatch(
    const GURL& url,
    bool is_committed,
    net::EffectiveConnectionType ect) const {
  if (!hints_)
    return;

  hints_->LogHintCacheMatch(url, is_committed, ect);
}

void PreviewsOptimizationGuide::OnHintsProcessed(
    const optimization_guide::proto::Configuration& config,
    const optimization_guide::ComponentInfo& info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PreviewsHints::CreateFromConfig, config, info),
      base::BindOnce(&PreviewsOptimizationGuide::UpdateHints,
                     io_weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsOptimizationGuide::UpdateHints(
    std::unique_ptr<PreviewsHints> hints) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  hints_ = std::move(hints);
  if (hints_)
    hints_->Initialize();
}

}  // namespace previews
