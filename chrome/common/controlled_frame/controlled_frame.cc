// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/controlled_frame/controlled_frame.h"

#include <string>

#include "base/containers/contains.h"
#include "chrome/common/initialize_extensions_client.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

namespace controlled_frame {

bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::Feature::Context context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       const extensions::ContextData& context_data) {
  // Verify that Controlled Frame and IWAs are enabled and ensure the invoking
  // context is correct.
  if (!base::FeatureList::IsEnabled(features::kIwaControlledFrame) ||
      !base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return false;
  }

  // Verify that the current context is an Isolated Web App and the API name is
  // in our expected list.
  bool is_not_extension = !extension && url.SchemeIs("isolated-app");
  return is_not_extension && context == extensions::Feature::WEB_PAGE_CONTEXT &&
         context_data.IsIsolatedApplication() &&
         base::Contains(GetControlledFrameFeatureList(), api_full_name);
}

extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CreateAvailabilityCheckMap() {
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap map;
  for (const auto* item : GetControlledFrameFeatureList()) {
    map.emplace(item, base::BindRepeating(&AvailabilityCheck));
  }
  return map;
}

}  // namespace controlled_frame
