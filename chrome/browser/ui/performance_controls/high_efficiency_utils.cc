// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
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

}  //  namespace high_efficiency
