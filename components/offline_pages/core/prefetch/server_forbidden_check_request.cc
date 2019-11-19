// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/server_forbidden_check_request.h"

namespace offline_pages {

namespace {
void OnGeneratePageBundleResponse(PrefService* pref_service,
                                  PrefetchService* prefetch_service,
                                  PrefetchRequestStatus status,
                                  const std::string& operation_name,
                                  const std::vector<RenderPageInfo>& pages) {
  if (status == PrefetchRequestStatus::kSuccess ||
      status == PrefetchRequestStatus::kEmptyRequestSuccess) {
    if (prefetch_service) {
      if (prefetch_service->GetLogger()) {
        prefetch_service->GetLogger()->RecordActivity(
            "Server-enabled check: prefetching allowed by server.");
      }
      // Request succeeded; enable prefetching.
      prefetch_service->SetEnabledByServer(pref_service, true);
    }
    return;
  }
  // In the case of some error that isn't ForbiddenByOPS, do nothing and allow
  // the check to be run again.
  if (prefetch_service && prefetch_service->GetLogger()) {
    prefetch_service->GetLogger()->RecordActivity(
        "Server-enabled check: prefetching not allowed by server.");
  }
}
}  // namespace

void CheckIfEnabledByServer(PrefService* pref_service,
                            PrefetchService* prefetch_service) {
  // Make a GeneratePageBundle request for no pages.
  prefetch_service->GetPrefetchNetworkRequestFactory()
      ->MakeGeneratePageBundleRequest(
          std::vector<std::string>(), std::string(),
          base::BindOnce(&OnGeneratePageBundleResponse, pref_service,
                         prefetch_service));
}

}  // namespace offline_pages
