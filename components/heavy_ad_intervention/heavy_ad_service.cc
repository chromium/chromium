// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/blocklist/opt_out_blocklist/sql/opt_out_store_sql.h"
#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace heavy_ad_intervention {

namespace {

const base::FilePath::CharType kHeavyAdInterventionOptOutDBFilename[] =
    FILE_PATH_LITERAL("heavy_ad_intervention_opt_out.db");

}  // namespace

// Whether an opt out store should be used or not.
bool HeavyAdOptOutStoreDisabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kHeavyAdPrivacyMitigations, "OptOutStoreDisabled", false);
}

HeavyAdService::HeavyAdService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

HeavyAdService::~HeavyAdService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void HeavyAdService::Initialize(const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!profile_path.empty());

  if (!base::FeatureList::IsEnabled(features::kHeavyAdPrivacyMitigations))
    return;

  std::unique_ptr<blocklist::OptOutStoreSQL> opt_out_store;
  if (!HeavyAdOptOutStoreDisabled()) {
    // Get the background thread to run SQLite on.
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    opt_out_store = std::make_unique<blocklist::OptOutStoreSQL>(
        content::GetUIThreadTaskRunner({}), background_task_runner,
        profile_path.Append(kHeavyAdInterventionOptOutDBFilename));
  }

  heavy_ad_blocklist_ = std::make_unique<HeavyAdBlocklist>(
      std::move(opt_out_store), base::DefaultClock::GetInstance(), this);
}

void HeavyAdService::InitializeOffTheRecord() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kHeavyAdPrivacyMitigations))
    return;

  // Providing a null out_out_store which sets up the blocklist in-memory only.
  heavy_ad_blocklist_ = std::make_unique<HeavyAdBlocklist>(
      nullptr /* opt_out_store */, base::DefaultClock::GetInstance(), this);
}

void HeavyAdService::NotifyOnBlocklistLoaded(
    base::OnceClosure on_blocklist_loaded_callback) {
  if (blocklist_is_loaded_) {
    std::move(on_blocklist_loaded_callback).Run();
    return;
  }

  on_blocklist_loaded_callback_ = std::move(on_blocklist_loaded_callback);
}

void HeavyAdService::NotifyOnBlocklistCleared(
    base::OnceClosure on_blocklist_cleared_callback) {
  on_blocklist_cleared_callback_ = std::move(on_blocklist_cleared_callback);
}

void HeavyAdService::OnLoadingStateChanged(bool is_loaded) {
  blocklist_is_loaded_ = is_loaded;

  if (blocklist_is_loaded_ && !on_blocklist_loaded_callback_.is_null())
    std::move(on_blocklist_loaded_callback_).Run();
}

void HeavyAdService::OnBlocklistCleared(base::Time time) {
  if (!on_blocklist_cleared_callback_.is_null())
    std::move(on_blocklist_cleared_callback_).Run();
}

}  // namespace heavy_ad_intervention
