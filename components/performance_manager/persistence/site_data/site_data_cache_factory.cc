// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/non_recording_site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {
SiteDataCacheFactory* g_instance = nullptr;
}  // namespace

SiteDataCacheFactory::SiteDataCacheFactory() {
  DCHECK(!g_instance);
  g_instance = this;
}

SiteDataCacheFactory::~SiteDataCacheFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
SiteDataCacheFactory* SiteDataCacheFactory::GetInstance() {
  return g_instance;
}

SiteDataCache* SiteDataCacheFactory::GetDataCacheForBrowserContext(
    const base::UnguessableToken& browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_map_.find(browser_context_id);
  if (it != data_cache_map_.end())
    return it->second.get();
  return nullptr;
}

bool SiteDataCacheFactory::IsDataCacheRecordingForTesting(
    const base::UnguessableToken& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_map_.find(browser_context_id);
  CHECK(it != data_cache_map_.end());
  return it->second->IsRecording();
}

void SiteDataCacheFactory::SetCacheForTesting(
    const base::UnguessableToken& browser_context_id,
    std::unique_ptr<SiteDataCache> cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_cache_map_.erase(browser_context_id);
  data_cache_map_.emplace(browser_context_id, std::move(cache));
}

void SiteDataCacheFactory::OnBrowserContextCreated(
    const base::UnguessableToken& browser_context_id,
    const base::FilePath& context_path,
    std::optional<base::UnguessableToken> parent_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!data_cache_map_.contains(browser_context_id));

  if (parent_context_id) {
    DCHECK(data_cache_map_.contains(parent_context_id.value()));
    SiteDataCache* data_cache_for_readers =
        data_cache_map_[parent_context_id.value()].get();
    DCHECK(data_cache_for_readers);
    data_cache_map_.emplace(
        browser_context_id,
        std::make_unique<NonRecordingSiteDataCache>(*data_cache_for_readers));
  } else {
    data_cache_map_.emplace(browser_context_id,
                            std::make_unique<SiteDataCacheImpl>(context_path));
  }
}

void SiteDataCacheFactory::OnBrowserContextDestroyed(
    const base::UnguessableToken& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_cache_map_.contains(browser_context_id));
  data_cache_map_.erase(browser_context_id);
}

}  // namespace performance_manager
