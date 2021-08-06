// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_proxy.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_handler.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using content::BrowserThread;

namespace quota_internals {

QuotaInternalsProxy::QuotaInternalsProxy(QuotaInternalsHandler* handler)
    : handler_(handler) {}

void QuotaInternalsProxy::RequestInfo(
    scoped_refptr<storage::QuotaManager> quota_manager) {
  DCHECK(quota_manager.get());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaInternalsProxy::RequestInfo, this, quota_manager));
    return;
  }
  quota_manager_ = quota_manager;

  quota_manager_->GetQuotaSettings(base::BindOnce(
      &QuotaInternalsProxy::DidGetSettings, weak_factory_.GetWeakPtr()));

  quota_manager_->GetStorageCapacity(base::BindOnce(
      &QuotaInternalsProxy::DidGetCapacity, weak_factory_.GetWeakPtr()));

  quota_manager_->GetGlobalUsage(
      StorageType::kTemporary,
      base::BindOnce(&QuotaInternalsProxy::DidGetGlobalUsage,
                     weak_factory_.GetWeakPtr(), StorageType::kTemporary));

  quota_manager_->GetGlobalUsage(
      StorageType::kPersistent,
      base::BindOnce(&QuotaInternalsProxy::DidGetGlobalUsage,
                     weak_factory_.GetWeakPtr(), StorageType::kPersistent));

  quota_manager_->GetGlobalUsage(
      StorageType::kSyncable,
      base::BindOnce(&QuotaInternalsProxy::DidGetGlobalUsage,
                     weak_factory_.GetWeakPtr(), StorageType::kSyncable));

  quota_manager_->DumpQuotaTable(base::BindOnce(
      &QuotaInternalsProxy::DidDumpQuotaTable, weak_factory_.GetWeakPtr()));

  quota_manager_->DumpBucketTable(base::BindOnce(
      &QuotaInternalsProxy::DidDumpBucketTable, weak_factory_.GetWeakPtr()));

  std::map<std::string, std::string> stats = quota_manager_->GetStatistics();
  ReportStatistics(stats);
}

void QuotaInternalsProxy::TriggerStoragePressure(
    url::Origin origin,
    scoped_refptr<storage::QuotaManager> quota_manager) {
  DCHECK(quota_manager.get());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&QuotaInternalsProxy::TriggerStoragePressure,
                                  this, origin, quota_manager));
    return;
  }
  quota_manager->SimulateStoragePressure(blink::StorageKey(origin));
}

QuotaInternalsProxy::~QuotaInternalsProxy() = default;

#define RELAY_TO_HANDLER(func, arg_t)                                        \
  void QuotaInternalsProxy::func(arg_t arg) {                                \
    if (!handler_)                                                           \
      return;                                                                \
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {                    \
      content::GetUIThreadTaskRunner({})->PostTask(                          \
          FROM_HERE, base::BindOnce(&QuotaInternalsProxy::func, this, arg)); \
      return;                                                                \
    }                                                                        \
                                                                             \
    handler_->func(arg);                                                     \
  }

RELAY_TO_HANDLER(ReportAvailableSpace, int64_t)
RELAY_TO_HANDLER(ReportGlobalInfo, const GlobalStorageInfo&)
RELAY_TO_HANDLER(ReportPerHostInfo, const std::vector<PerHostStorageInfo>&)
RELAY_TO_HANDLER(ReportPerOriginInfo, const std::vector<PerOriginStorageInfo>&)
RELAY_TO_HANDLER(ReportStatistics, const Statistics&)

#undef RELAY_TO_HANDLER

void QuotaInternalsProxy::DidGetSettings(
    const storage::QuotaSettings& settings) {
  // TODO(michaeln): also report the other config fields
  GlobalStorageInfo info(StorageType::kTemporary);
  info.set_quota(settings.pool_size);
  ReportGlobalInfo(info);
}

void QuotaInternalsProxy::DidGetCapacity(int64_t total_space,
                                         int64_t available_space) {
  // TODO(michaeln): also report total_space
  ReportAvailableSpace(available_space);
}

void QuotaInternalsProxy::DidGetGlobalUsage(StorageType type,
                                            int64_t usage,
                                            int64_t unlimited_usage) {
  GlobalStorageInfo info(type);
  info.set_usage(usage);
  info.set_unlimited_usage(unlimited_usage);

  ReportGlobalInfo(info);
  RequestPerOriginInfo(type);
}

void QuotaInternalsProxy::DidDumpQuotaTable(const QuotaTableEntries& entries) {
  std::vector<PerHostStorageInfo> host_info;
  host_info.reserve(entries.size());

  for (const auto& entry : entries) {
    PerHostStorageInfo info(entry.host, entry.type);
    info.set_quota(entry.quota);
    host_info.push_back(info);
  }

  ReportPerHostInfo(host_info);
}

void QuotaInternalsProxy::DidDumpBucketTable(
    const BucketTableEntries& entries) {
  std::vector<PerOriginStorageInfo> origin_info;
  origin_info.reserve(entries.size());

  for (const auto& entry : entries) {
    PerOriginStorageInfo info(entry.storage_key.origin().GetURL(), entry.type);
    info.set_used_count(entry.use_count);
    info.set_last_access_time(entry.last_accessed);
    info.set_last_modified_time(entry.last_modified);

    origin_info.push_back(info);
  }

  ReportPerOriginInfo(origin_info);
}

void QuotaInternalsProxy::DidGetHostUsage(
    const std::string& host,
    StorageType type,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK(type == StorageType::kTemporary || type == StorageType::kPersistent ||
         type == StorageType::kSyncable);

  PerHostStorageInfo info(host, type);
  info.set_usage(usage);

  report_pending_.push_back(info);
  hosts_pending_.erase(make_pair(host, type));
  if (report_pending_.size() >= 10 || hosts_pending_.empty()) {
    ReportPerHostInfo(report_pending_);
    report_pending_.clear();
  }

  if (!hosts_pending_.empty())
    GetHostUsage(hosts_pending_.begin()->first,
                 hosts_pending_.begin()->second);
}

void QuotaInternalsProxy::RequestPerOriginInfo(StorageType type) {
  DCHECK(quota_manager_.get());

  std::set<blink::StorageKey> storage_keys =
      quota_manager_->GetCachedStorageKeys(type);

  std::vector<PerOriginStorageInfo> origin_info;
  origin_info.reserve(storage_keys.size());

  std::set<std::string> hosts;
  std::vector<PerHostStorageInfo> host_info;

  for (const blink::StorageKey& storage_key : storage_keys) {
    PerOriginStorageInfo info(storage_key.origin().GetURL(), type);
    origin_info.push_back(info);

    const std::string& host = storage_key.origin().host();
    if (hosts.insert(host).second) {
      PerHostStorageInfo info(host, type);
      host_info.push_back(info);
      VisitHost(host, type);
    }
  }
  ReportPerOriginInfo(origin_info);
  ReportPerHostInfo(host_info);
}

void QuotaInternalsProxy::VisitHost(const std::string& host, StorageType type) {
  if (hosts_visited_.insert(std::make_pair(host, type)).second) {
    hosts_pending_.insert(std::make_pair(host, type));
    if (hosts_pending_.size() == 1) {
      GetHostUsage(host, type);
    }
  }
}

void QuotaInternalsProxy::GetHostUsage(const std::string& host,
                                       StorageType type) {
  DCHECK(quota_manager_.get());
  quota_manager_->GetHostUsageWithBreakdown(
      host, type,
      base::BindOnce(&QuotaInternalsProxy::DidGetHostUsage,
                     weak_factory_.GetWeakPtr(), host, type));
}

}  // namespace quota_internals
