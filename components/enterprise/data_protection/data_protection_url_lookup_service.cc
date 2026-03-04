// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/data_protection_url_lookup_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"

namespace {

constexpr char kVerdictCacheEventHistogram[] =
    "Enterprise.DataProtection.VerdictCacheEvent";

int GetCacheDurationSec(safe_browsing::RTLookupResponse* rt_lookup_response) {
  DCHECK(rt_lookup_response);
  const auto& threat_infos = rt_lookup_response->threat_info();

  // Defensive check
  if (threat_infos.empty()) {
    return 0;
  }

  // We do not check for matched rules, because that would exclude safe verdicts
  int cache_duration_sec = threat_infos[0].cache_duration_sec();
  for (int i = 1; i < threat_infos.size(); ++i) {
    const auto& threat_info = threat_infos[i];
    if (threat_info.cache_duration_sec() < cache_duration_sec) {
      cache_duration_sec = threat_info.cache_duration_sec();
    }
  }
  return cache_duration_sec;
}

}  // namespace
namespace enterprise_data_protection {

const size_t kVerdictCacheMaxSize = 200;

DataProtectionUrlLookupService::Verdict::Verdict() = default;
DataProtectionUrlLookupService::Verdict::Verdict(Verdict&&) = default;
DataProtectionUrlLookupService::Verdict::~Verdict() = default;

DataProtectionUrlLookupService::DataProtectionUrlLookupService()
    : verdict_cache_(GetVerdictCacheMaxSize()) {}

DataProtectionUrlLookupService::~DataProtectionUrlLookupService() = default;

void DataProtectionUrlLookupService::DoLookup(
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
    const GURL& url,
    LookupCallback callback,
    SessionID session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  auto cached_verdict = verdict_cache_.Peek(url.spec());
  if (cached_verdict != verdict_cache_.end() &&
      !IsVerdictExpired(cached_verdict->second)) {
    // Proto assignment has deep copy semantics. There is room to optimize by
    // implementing shared ownership (both this service and
    // `DataProtectionPageUserData` own a ptr to RTLookupResponse).
    std::unique_ptr<safe_browsing::RTLookupResponse> response =
        std::make_unique<safe_browsing::RTLookupResponse>(
            *cached_verdict->second.response);
    std::move(callback).Run(std::move(response));
    base::UmaHistogramEnumeration(kVerdictCacheEventHistogram,
                                  URLVerdictCacheEvent::kCacheHit);
    return;
  }

  base::UmaHistogramEnumeration(kVerdictCacheEventHistogram,
                                URLVerdictCacheEvent::kUrlScanRequest);
  lookup_service->StartMaybeCachedLookup(
      url,
      base::BindOnce(&DataProtectionUrlLookupService::OnRealTimeLookupComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), url),
      base::SequencedTaskRunner::GetCurrentDefault(), session_id,
      /*referring_app_info=*/std::nullopt, /*use_cache=*/
      false);
}

void DataProtectionUrlLookupService::OnRealTimeLookupComplete(
    LookupCallback callback,
    const GURL& url,
    bool is_success,
    bool is_cached,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_success) {
    rt_lookup_response.reset();
  } else if (rt_lookup_response) {
    // Guarantee that verdict cache contents are non-empty.
    int cache_duration_sec = GetCacheDurationSec(rt_lookup_response.get());
    if (cache_duration_sec > 0) {
      Verdict verdict;
      verdict.response = std::make_unique<safe_browsing::RTLookupResponse>(
          *rt_lookup_response);
      verdict.expiry_time =
          base::Time::Now() + base::Seconds(cache_duration_sec);
      verdict_cache_.Put(url.spec(), std::move(verdict));
    }
  }

  std::move(callback).Run(std::move(rt_lookup_response));
}

// static
bool DataProtectionUrlLookupService::IsVerdictExpired(const Verdict& verdict) {
  return base::Time::Now() > verdict.expiry_time;
}

// static
size_t DataProtectionUrlLookupService::GetVerdictCacheMaxSize() {
  return kVerdictCacheMaxSize;
}

}  // namespace enterprise_data_protection
