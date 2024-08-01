// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/common/url_constants.h"

namespace memory_saver {

bool IsURLSupported(GURL url) {
  return !url.SchemeIs(content::kChromeUIScheme);
}

std::optional<::mojom::LifecycleUnitDiscardReason> GetDiscardReason(
    content::WebContents* contents) {
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(contents);
  return pre_discard_resource_usage == nullptr
             ? std::nullopt
             : std::make_optional<::mojom::LifecycleUnitDiscardReason>(
                   pre_discard_resource_usage->discard_reason());
}

int64_t GetDiscardedMemorySavingsInBytes(content::WebContents* contents) {
  const auto* const pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(contents);
  if (pre_discard_resource_usage == nullptr) {
    return 0;
  } else {
    CHECK(pre_discard_resource_usage->memory_footprint_estimate_kb() >= 0);
    return pre_discard_resource_usage->memory_footprint_estimate_kb() * 1024;
  }
}

}  // namespace memory_saver
