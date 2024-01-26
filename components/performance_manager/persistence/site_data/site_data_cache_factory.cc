// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/non_recording_site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
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
  // Clear the cache map before unsetting |g_instance| as this will cause some
  // calls to |SetDataCacheInspectorForBrowserContext|.
  data_cache_map_.clear();
  for (const auto& iter : data_cache_map_)
    DCHECK_EQ(0, iter.second->Size());
  g_instance = nullptr;
}

// static
SiteDataCacheFactory* SiteDataCacheFactory::GetInstance() {
  return g_instance;
}

SiteDataCache* SiteDataCacheFactory::GetDataCacheForBrowserContext(
    const std::string& browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_map_.find(browser_context_id);
  if (it != data_cache_map_.end())
    return it->second.get();
  return nullptr;
}

SiteDataCacheInspector* SiteDataCacheFactory::GetInspectorForBrowserContext(
    const std::string& browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_inspector_map_.find(browser_context_id);
  if (it != data_cache_inspector_map_.end())
    return it->second;
  return nullptr;
}

void SiteDataCacheFactory::SetDataCacheInspectorForBrowserContext(
    SiteDataCacheInspector* inspector,
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (inspector) {
    DCHECK_EQ(nullptr, GetInspectorForBrowserContext(browser_context_id));
    data_cache_inspector_map_.emplace(
        std::make_pair(browser_context_id, inspector));
  } else {
    DCHECK_NE(nullptr, GetInspectorForBrowserContext(browser_context_id));
    data_cache_inspector_map_.erase(browser_context_id);
  }
}

bool SiteDataCacheFactory::IsDataCacheRecordingForTesting(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_map_.find(browser_context_id);
  CHECK(it != data_cache_map_.end());
  return it->second->IsRecording();
}

void SiteDataCacheFactory::SetCacheForTesting(
    const std::string& browser_context_id,
    std::unique_ptr<SiteDataCache> cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_cache_map_.erase(browser_context_id);
  data_cache_map_.emplace(browser_context_id, std::move(cache));
}

void SiteDataCacheFactory::SetCacheInspectorForTesting(
    const std::string& browser_context_id,
    SiteDataCacheInspector* inspector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(data_cache_inspector_map_, browser_context_id));
  data_cache_inspector_map_.emplace(browser_context_id, inspector);
}

void SiteDataCacheFactory::OnBrowserContextCreated(
    const std::string& browser_context_id,
    const base::FilePath& context_path,
    std::optional<std::string> parent_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!base::Contains(data_cache_map_, browser_context_id));

  if (parent_context_id) {
    SiteDataCacheInspector* parent_debug =
        GetInspectorForBrowserContext(parent_context_id.value());
    DCHECK(parent_debug);
    DCHECK(base::Contains(data_cache_map_, parent_context_id.value()));
    SiteDataCache* data_cache_for_readers =
        data_cache_map_[parent_context_id.value()].get();
    DCHECK(data_cache_for_readers);
    data_cache_map_.emplace(
        std::move(browser_context_id),
        std::make_unique<NonRecordingSiteDataCache>(
            browser_context_id, parent_debug, data_cache_for_readers));
  } else {
    data_cache_map_.emplace(
        std::move(browser_context_id),
        std::make_unique<SiteDataCacheImpl>(browser_context_id, context_path));
  }
}

void SiteDataCacheFactory::OnBrowserContextDestroyed(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(data_cache_map_, browser_context_id));
  data_cache_map_.erase(browser_context_id);
}

}  // namespace performance_manager
