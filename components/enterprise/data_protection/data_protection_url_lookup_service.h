// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace enterprise_data_protection {

extern const size_t kVerdictCacheMaxSize;

using LookupCallback =
    base::OnceCallback<void(std::unique_ptr<safe_browsing::RTLookupResponse>)>;

// Service that contains a URL lookup verdict cache and scopes it to a profile
// TODO(447624248): consider combining with
// `ChromeEnterpriseRealTimeUrlLookupService`
class DataProtectionUrlLookupService : public KeyedService {
 public:
  DataProtectionUrlLookupService();
  ~DataProtectionUrlLookupService() override;

  void DoLookup(safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
                const GURL& url,
                LookupCallback callback,
                SessionID session_id);

  enum class URLVerdictCacheEvent {
    // Verdict obtained from cache.
    kCacheHit = 0,

    // Chrome made a URL scan request.
    kUrlScanRequest = 1,

    kMaxValue = kUrlScanRequest
  };

 private:
  struct Verdict {
    Verdict();
    Verdict(Verdict&&);
    ~Verdict();

    std::unique_ptr<safe_browsing::RTLookupResponse> response;
    base::Time expiry_time;
  };

  void OnRealTimeLookupComplete(
      LookupCallback callback,
      const GURL& url,
      bool is_success,
      bool is_cached,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  static size_t GetVerdictCacheMaxSize();

  static bool IsVerdictExpired(const Verdict& verdict);

  // cache which maps the full URL specification string to the safe-browsing
  // verdict, and its expiry time.
  base::LRUCache<std::string, Verdict> verdict_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataProtectionUrlLookupService> weak_factory_{this};
};

}  // namespace enterprise_data_protection

#endif  // COMPONENTS_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
