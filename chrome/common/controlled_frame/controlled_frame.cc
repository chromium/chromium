// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/controlled_frame/controlled_frame.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/initialize_extensions_client.h"
#include "chrome/common/url_constants.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "third_party/blink/public/common/features.h"

base::span<const char* const> GetControlledFrameFeatureList() {
  static constexpr const char* feature_list[] = {
      "controlledFrameInternal", "chromeWebViewInternal", "guestViewInternal",
      "webRequestInternal",      "webViewInternal",
  };
  return base::make_span(feature_list);
}

namespace controlled_frame {

// |AvailabilityCheck()| inspects the current environment to determine whether
// ControlledFrame or its dependencies should be made available to that
// environment. The function is configured using |CreateAvailabilityCheckMap()|
// which assigns the function to each of the Controlled Frame-associated
// features. Those features are defined in and provided by the extensions
// features system. For Controlled Frame to work in a given channel, all of the
// features Controlled Frame depends upon and Controlled Frame itself must be
// available in that channel. See |GetControlledFrameFeatureList()|.
//
// |AvailabilityCheck()| will be called by
// |SimpleFeature::IsAvailableToContextImpl()| once that function determines
// that the feature has a "delegated availability check" and runs the check that
// was installed by |CreateAvailabilityCheckMap()|.
//
// SimpleFeature is defined in //extensions/common/ and is called by code
// defined by //extensions/browser/ and //extensions/renderer/, appearing in
// call stacks that originate in either the browser or renderer processes. In
// the browser process, it may be called from contexts that have a
// RendererFrameHost or a RendererProcessHost. In the renderer process, it is
// called and checks for a process wide isolation setting and whether the
// isolation flag is enabled for the process.
bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::mojom::ContextType context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       const extensions::ContextData& context_data) {
  // Verify that the kControlledFrame blink::features flag is enabled. It's a
  // kill switch, so it should be enabled by default and only present so if
  // needed we can use Finch to disable Controlled Frame.
  if (!base::FeatureList::IsEnabled(blink::features::kControlledFrame)) {
    return false;
  }

  // Verify that the current context is an Isolated Web App and the API name is
  // in our expected list.
  return (extension == nullptr) && url.SchemeIs(chrome::kIsolatedAppScheme) &&
         context == extensions::mojom::ContextType::kWebPage &&
         context_data.HasControlledFrameCapability() &&
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
