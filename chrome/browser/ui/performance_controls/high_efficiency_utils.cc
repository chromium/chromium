// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"

#include "base/containers/contains.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/common/url_constants.h"

namespace high_efficiency {

bool IsURLSupported(GURL url) {
  return !url.SchemeIs(content::kChromeUIScheme);
}

absl::optional<::mojom::LifecycleUnitDiscardReason> GetDiscardReason(
    content::WebContents* contents) {
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(contents);
  return pre_discard_resource_usage == nullptr
             ? absl::nullopt
             : absl::make_optional<::mojom::LifecycleUnitDiscardReason>(
                   pre_discard_resource_usage->discard_reason());
}

bool IsSiteInExceptionsList(PrefService* pref_service,
                            const std::string& site) {
  const base::Value::List& discard_exception_list = pref_service->GetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions);
  return base::Contains(discard_exception_list, site);
}

void AddSiteToExceptionsList(PrefService* pref_service,
                             const std::string& site) {
  base::Value::List discard_exception_list =
      pref_service
          ->GetList(
              performance_manager::user_tuning::prefs::kTabDiscardingExceptions)
          .Clone();
  if (!base::Contains(discard_exception_list, site)) {
    discard_exception_list.Append(site);
    pref_service->SetList(
        performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
        std::move(discard_exception_list));
  }
}

void ClearSiteExceptionsList(PrefService* pref_service) {
  pref_service->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions, {});
}

uint64_t GetDiscardedMemorySavingsInBytes(content::WebContents* contents) {
  const auto* const pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(contents);
  return pre_discard_resource_usage == nullptr
             ? 0
             : pre_discard_resource_usage->memory_footprint_estimate_kb() *
                   1024;
}

}  //  namespace high_efficiency
