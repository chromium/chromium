// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_test_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "components/safe_browsing/db/util.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

V4ProtocolConfig GetTestV4ProtocolConfig(bool disable_auto_update) {
  return V4ProtocolConfig(kClient, disable_auto_update, kKeyParam, kAppVer);
}

std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta) {
  os << "{threat_pattern_type=" << static_cast<int>(meta.threat_pattern_type)
     << ", api_permissions=[";
  for (auto p : meta.api_permissions)
    os << p << ",";
  os << "subresource_filter_match=[";
  for (auto t : meta.subresource_filter_match)
    os << static_cast<int>(t.first) << ":" << static_cast<int>(t.second) << ",";
  return os << "], population_id=" << meta.population_id << "}";
}

TestV4Store::TestV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path)
    : V4Store(task_runner, store_path) {}

TestV4Store::~TestV4Store() = default;

bool TestV4Store::HasValidData() const {
  return true;
}

void TestV4Store::MarkPrefixAsBad(HashPrefix prefix) {
  auto& vec = mock_prefixes_[prefix.size()];
  vec.insert(std::upper_bound(vec.begin(), vec.end(), prefix), prefix);
  hash_prefix_map_[prefix.size()] = base::StrCat(vec);
}

void TestV4Store::SetPrefixes(std::vector<HashPrefix> prefixes,
                              PrefixSize size) {
  std::sort(prefixes.begin(), prefixes.end());
  mock_prefixes_[size] = prefixes;
  hash_prefix_map_[size] = base::StrCat(prefixes);
}

TestV4Database::TestV4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : V4Database(db_task_runner, std::move(store_map)) {}

void TestV4Database::MarkPrefixAsBad(ListIdentifier list_id,
                                     HashPrefix prefix) {
  V4Store* base_store = store_map_->at(list_id).get();
  TestV4Store* test_store = static_cast<TestV4Store*>(base_store);
  test_store->MarkPrefixAsBad(prefix);
}

TestV4StoreFactory::TestV4StoreFactory() = default;

TestV4StoreFactory::~TestV4StoreFactory() = default;

std::unique_ptr<V4Store> TestV4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  auto new_store = std::make_unique<TestV4Store>(task_runner, store_path);
  new_store->Initialize();
  return std::move(new_store);
}

TestV4DatabaseFactory::TestV4DatabaseFactory() = default;

TestV4DatabaseFactory::~TestV4DatabaseFactory() = default;

std::unique_ptr<V4Database> TestV4DatabaseFactory::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map) {
  auto v4_db =
      std::make_unique<TestV4Database>(db_task_runner, std::move(store_map));
  v4_db_ = v4_db.get();
  return std::move(v4_db);
}

void TestV4DatabaseFactory::MarkPrefixAsBad(ListIdentifier list_id,
                                            HashPrefix prefix) {
  v4_db_->MarkPrefixAsBad(list_id, prefix);
}

TestV4GetHashProtocolManager::TestV4GetHashProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config)
    : V4GetHashProtocolManager(url_loader_factory, stores_to_check, config) {}

void TestV4GetHashProtocolManager::AddToFullHashCache(FullHashInfo fhi) {
  full_hash_cache_[fhi.full_hash].full_hash_infos.push_back(fhi);
}

TestV4GetHashProtocolManagerFactory::TestV4GetHashProtocolManagerFactory() =
    default;

TestV4GetHashProtocolManagerFactory::~TestV4GetHashProtocolManagerFactory() =
    default;

std::unique_ptr<V4GetHashProtocolManager>
TestV4GetHashProtocolManagerFactory::CreateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config) {
  auto pm = std::make_unique<TestV4GetHashProtocolManager>(
      url_loader_factory, stores_to_check, config);
  pm_ = pm.get();
  return std::move(pm);
}

FullHashInfo GetFullHashInfo(const GURL& url, const ListIdentifier& list_id) {
  return FullHashInfo(V4ProtocolManagerUtil::GetFullHash(url), list_id,
                      base::Time::Now() + base::TimeDelta::FromMinutes(5));
}

FullHashInfo GetFullHashInfoWithMetadata(
    const GURL& url,
    const ListIdentifier& list_id,
    const ThreatMetadata& threat_metadata) {
  FullHashInfo fhi = GetFullHashInfo(url, list_id);
  fhi.metadata = threat_metadata;
  return fhi;
}

}  // namespace safe_browsing
