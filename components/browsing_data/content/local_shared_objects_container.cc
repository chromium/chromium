// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_shared_objects_container.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/browsing_data/content/appcache_helper.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/canonical_cookie_hash.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "components/browsing_data/content/shared_worker_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "url/gurl.h"

namespace browsing_data {
namespace {

bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl1, gurl2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

LocalSharedObjectsContainer::LocalSharedObjectsContainer(
    content::BrowserContext* browser_context,
    const std::vector<storage::FileSystemType>& additional_file_system_types,
    browsing_data::CookieHelper::IsDeletionDisabledCallback callback)
    : appcaches_(new CannedAppCacheHelper(
          browser_context->GetDefaultStoragePartition()->GetAppCacheService())),
      cookies_(
          new CannedCookieHelper(browser_context->GetDefaultStoragePartition(),
                                 std::move(callback))),
      databases_(new CannedDatabaseHelper(browser_context)),
      file_systems_(new CannedFileSystemHelper(
          browser_context->GetDefaultStoragePartition()->GetFileSystemContext(),
          additional_file_system_types,
          browser_context->GetDefaultStoragePartition()->GetNativeIOContext())),
      indexed_dbs_(new CannedIndexedDBHelper(
          browser_context->GetDefaultStoragePartition())),
      local_storages_(new CannedLocalStorageHelper(browser_context)),
      service_workers_(new CannedServiceWorkerHelper(
          browser_context->GetDefaultStoragePartition()
              ->GetServiceWorkerContext())),
      shared_workers_(new CannedSharedWorkerHelper(
          browser_context->GetDefaultStoragePartition())),
      cache_storages_(new CannedCacheStorageHelper(
          browser_context->GetDefaultStoragePartition())),
      session_storages_(new CannedLocalStorageHelper(browser_context)) {}

LocalSharedObjectsContainer::~LocalSharedObjectsContainer() = default;

size_t LocalSharedObjectsContainer::GetObjectCount() const {
  size_t count = 0;
  count += appcaches()->GetCount();
  count += cookies()->GetCookieCount();
  count += databases()->GetCount();
  count += file_systems()->GetCount();
  count += indexed_dbs()->GetCount();
  count += local_storages()->GetCount();
  count += service_workers()->GetCount();
  count += shared_workers()->GetSharedWorkerCount();
  count += cache_storages()->GetCount();
  count += session_storages()->GetCount();
  return count;
}

size_t LocalSharedObjectsContainer::GetObjectCountForDomain(
    const GURL& origin) const {
  size_t count = 0;

  // Count all cookies that have the same domain as the provided |origin|. This
  // means count all cookies that have been set by a host that is not considered
  // to be a third party regarding the domain of the provided |origin|. E.g. if
  // the origin is "http://foo.com" then all cookies with domain foo.com,
  // a.foo.com, b.a.foo.com or *.foo.com will be counted.
  typedef CannedCookieHelper::OriginCookieSetMap OriginCookieSetMap;
  const OriginCookieSetMap& origin_cookies_set_map =
      cookies()->origin_cookie_set_map();
  for (auto it = origin_cookies_set_map.begin();
       it != origin_cookies_set_map.end(); ++it) {
    const canonical_cookie::CookieHashSet* cookie_list = it->second.get();
    for (const auto& cookie : *cookie_list) {
      // The |domain_url| is only created in order to use the
      // SameDomainOrHost method below. It does not matter which scheme is
      // used as the scheme is ignored by the SameDomainOrHost method.
      GURL domain_url = net::cookie_util::CookieOriginToURL(
          cookie.Domain(), false /* is_https */);

      if (origin.SchemeIsHTTPOrHTTPS() && SameDomainOrHost(origin, domain_url))
        ++count;
    }
  }

  // Count local storages for the domain of the given |origin|.
  for (const auto& storage_origin : local_storages()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count session storages for the domain of the given |origin|.
  for (const auto& storage_origin : session_storages()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count indexed dbs for the domain of the given |origin|.
  for (const auto& storage_origin : indexed_dbs()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count service workers for the domain of the given |origin|.
  for (const auto& storage_origin : service_workers()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count shared workers for the domain of the given |origin|.
  typedef SharedWorkerHelper::SharedWorkerInfo SharedWorkerInfo;
  const std::set<SharedWorkerInfo>& shared_worker_info =
      shared_workers()->GetSharedWorkerInfo();
  for (const auto& it : shared_worker_info) {
    if (SameDomainOrHost(origin, it.worker))
      ++count;
  }

  // Count cache storages for the domain of the given |origin|.
  for (const auto& storage_origin : cache_storages()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count filesystems for the domain of the given |origin|.
  for (const auto& storage_origin : file_systems()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count databases for the domain of the given |origin|.
  for (const auto& storage_origin : databases()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  // Count the AppCache manifest files for the domain of the given |origin|.
  for (const auto& storage_origin : appcaches()->GetOrigins()) {
    if (SameDomainOrHost(origin, storage_origin.GetURL()))
      ++count;
  }

  return count;
}

size_t LocalSharedObjectsContainer::GetDomainCount() const {
  std::set<base::StringPiece> hosts;

  for (const auto& it : cookies()->origin_cookie_set_map()) {
    for (const auto& cookie : *it.second) {
      hosts.insert(cookie.Domain());
    }
  }

  for (const auto& origin : local_storages()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : session_storages()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : indexed_dbs()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : service_workers()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& info : shared_workers()->GetSharedWorkerInfo())
    hosts.insert(info.storage_key.origin().host());

  for (const auto& origin : cache_storages()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : file_systems()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : databases()->GetOrigins())
    hosts.insert(origin.host());

  for (const auto& origin : appcaches()->GetOrigins())
    hosts.insert(origin.host());

  std::set<std::string> domains;
  for (const base::StringPiece& host : hosts) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!domain.empty())
      domains.insert(std::move(domain));
    else
      domains.insert(std::string(host));
  }
  return domains.size();
}

void LocalSharedObjectsContainer::Reset() {
  appcaches_->Reset();
  cookies_->Reset();
  databases_->Reset();
  file_systems_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  service_workers_->Reset();
  shared_workers_->Reset();
  cache_storages_->Reset();
  session_storages_->Reset();
}

}  // namespace browsing_data
