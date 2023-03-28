// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/controlled_frame.h"

#include <string>

#include "base/containers/contains.h"
#include "chrome/common/initialize_extensions_client.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

class GURL;

namespace controlled_frame {

bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::Feature::Context context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       std::unique_ptr<extensions::ContextData> context_data) {
  // Verify that Controlled Frame and IWAs are enabled and ensure the invoking
  // context is correct.
  if (!base::FeatureList::IsEnabled(features::kIwaControlledFrame) ||
      !base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return false;
  }

  // If |context_data| isn't set, then this can't be an IWA. Default to turning
  // off Controlled Frame. In the future, if //extensions can guarantee a
  // |context_data| is always passed, this should become a CHECK(context_data)
  // and instead rely on the ->IsIsolatedApplication() call to verify the
  // invoking context isn't an IWA.
  if (!context_data) {
    return false;
  }

  // Verify that the app is isolated and the API name is in our expected list.
  return context_data->IsIsolatedApplication() &&
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
