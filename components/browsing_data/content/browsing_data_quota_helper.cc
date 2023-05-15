// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_quota_helper.h"

#include "base/location.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo() = default;

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(
    const blink::StorageKey& storage_key)
    : storage_key(storage_key) {}

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(
    const blink::StorageKey& storage_key,
    int64_t temporary_usage,
    int64_t syncable_usage)
    : storage_key(storage_key),
      temporary_usage(temporary_usage),
      syncable_usage(syncable_usage) {}

BrowsingDataQuotaHelper::QuotaInfo::~QuotaInfo() = default;

// static
void BrowsingDataQuotaHelperDeleter::Destruct(
    const BrowsingDataQuotaHelper* helper) {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, helper);
}

BrowsingDataQuotaHelper::BrowsingDataQuotaHelper() = default;

BrowsingDataQuotaHelper::~BrowsingDataQuotaHelper() = default;

bool BrowsingDataQuotaHelper::QuotaInfo::operator<(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  return std::tie(storage_key, temporary_usage, syncable_usage) <
         std::tie(rhs.storage_key, rhs.temporary_usage, rhs.syncable_usage);
}

bool BrowsingDataQuotaHelper::QuotaInfo::operator==(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  return std::tie(storage_key, temporary_usage, syncable_usage) ==
         std::tie(rhs.storage_key, rhs.temporary_usage, rhs.syncable_usage);
}
