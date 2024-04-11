// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"

#include <set>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

namespace performance_manager {

namespace {

constexpr char kDataStoreDBName[] = "Site Characteristics Database";

}  // namespace

SiteDataCacheImpl::SiteDataCacheImpl(const std::string& browser_context_id,
                                     const base::FilePath& browser_context_path)
    : browser_context_id_(browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_store_ = std::make_unique<LevelDBSiteDataStore>(
      browser_context_path.AppendASCII(kDataStoreDBName));

  // Register the debug interface against the browser context. The factory
  // should always exist by the time this is called, since it is created once
  // there's a browser context with keyed services enabled.
  auto* factory = SiteDataCacheFactory::GetInstance();
  CHECK(factory);
  factory->SetDataCacheInspectorForBrowserContext(this, browser_context_id_);
}

SiteDataCacheImpl::SiteDataCacheImpl(const std::string& browser_context_id)
    : browser_context_id_(browser_context_id) {}

SiteDataCacheImpl::~SiteDataCacheImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SiteDataCacheFactory::GetInstance()->SetDataCacheInspectorForBrowserContext(
      nullptr, browser_context_id_);
}

std::unique_ptr<SiteDataReader> SiteDataCacheImpl::GetReaderForOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internal::SiteDataImpl* impl = GetOrCreateFeatureImpl(origin);
  DCHECK(impl);
  SiteDataReader* data_reader = new SiteDataReaderImpl(impl);
  return base::WrapUnique(data_reader);
}

std::unique_ptr<SiteDataWriter> SiteDataCacheImpl::GetWriterForOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internal::SiteDataImpl* impl = GetOrCreateFeatureImpl(origin);
  DCHECK(impl);
  SiteDataWriter* data_writer = new SiteDataWriter(impl);
  return base::WrapUnique(data_writer);
}

bool SiteDataCacheImpl::IsRecording() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

int SiteDataCacheImpl::Size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return origin_data_map_.size();
}

const char* SiteDataCacheImpl::GetDataCacheName() {
  return "SiteDataCache";
}

std::vector<url::Origin> SiteDataCacheImpl::GetAllInMemoryOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<url::Origin> ret;

  ret.reserve(origin_data_map_.size());
  for (const auto& entry : origin_data_map_)
    ret.push_back(entry.first);

  return ret;
}

void SiteDataCacheImpl::GetDataStoreSize(DataStoreSizeCallback on_have_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_store_->GetStoreSize(std::move(on_have_data));
}

bool SiteDataCacheImpl::GetDataForOrigin(const url::Origin& origin,
                                         bool* is_dirty,
                                         std::unique_ptr<SiteDataProto>* data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nullptr, data);
  const auto it = origin_data_map_.find(origin);
  if (it == origin_data_map_.end())
    return false;

  std::unique_ptr<SiteDataProto> ret = std::make_unique<SiteDataProto>();
  ret->CopyFrom(it->second->FlushStateToProto());
  *is_dirty = it->second->is_dirty();
  *data = std::move(ret);
  return true;
}

SiteDataCacheImpl* SiteDataCacheImpl::GetDataCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

internal::SiteDataImpl* SiteDataCacheImpl::GetOrCreateFeatureImpl(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Start by checking if there's already an entry for this origin.
  auto iter = origin_data_map_.find(origin);
  if (iter != origin_data_map_.end())
    return iter->second;

  // If not create a new one and add it to the map.
  internal::SiteDataImpl* site_data = new internal::SiteDataImpl(
      origin, weak_factory_.GetWeakPtr(), data_store_.get());

  // internal::SiteDataImpl is a ref-counted object, it's safe to store a raw
  // pointer to it here as this class will get notified when it's about to be
  // destroyed and it'll be removed from the map.
  origin_data_map_.insert(std::make_pair(origin, site_data));
  return site_data;
}

void SiteDataCacheImpl::OnSiteDataImplDestroyed(internal::SiteDataImpl* impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(impl);
  DCHECK(base::Contains(origin_data_map_, impl->origin()));
  // Remove the entry for this origin as this is about to get destroyed.
  auto num_erased = origin_data_map_.erase(impl->origin());
  DCHECK_EQ(1U, num_erased);
}

void SiteDataCacheImpl::ClearSiteDataForOrigins(
    const std::vector<url::Origin>& origins_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's not necessary to invalidate the pending DB write operations as they
  // run on a sequenced task and so it's guaranteed that the remove operations
  // posted here will run after any other pending operation.
  for (const auto& it : origins_to_remove) {
    auto map_iter = origin_data_map_.find(it);
    if (map_iter != origin_data_map_.end())
      map_iter->second->ClearObservationsAndInvalidateReadOperation();
  }

  data_store_->RemoveSiteDataFromStore(origins_to_remove);
}

void SiteDataCacheImpl::ClearAllSiteData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's not necessary to invalidate the pending DB write operations as they
  // run on a sequenced task and so it's guaranteed that the remove operations
  // posted here will run after any other pending operation.
  for (auto& data : origin_data_map_)
    data.second->ClearObservationsAndInvalidateReadOperation();
  data_store_->ClearStore();
}

void SiteDataCacheImpl::SetInitializationCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_store_->SetInitializationCallbackForTesting(std::move(callback));
}

}  // namespace performance_manager
