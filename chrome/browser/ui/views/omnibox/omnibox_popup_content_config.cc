// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_config.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "components/omnibox/common/omnibox_features.h"

// static
OmniboxPopupContentConfig OmniboxPopupContentConfig::ForClassic() {
  OmniboxPopupContentConfig config;
  config.evict_on_hide =
      base::FeatureList::IsEnabled(omnibox::kOmniboxWebUIEvictOnHide);
  config.detach_on_hide = base::FeatureList::IsEnabled(
      omnibox::kOmniboxWebUIDetachWebContentsOnHide);
  config.draw_shadow_in_webui = false;
  config.debounce_resize =
      base::FeatureList::IsEnabled(omnibox::kOmniboxWebUIDebounceResize);
  config.height_workarounds =
      base::FeatureList::IsEnabled(omnibox::kOmniboxWebUIHeightWorkarounds);
  config.size_webview_to_preferred_height = base::FeatureList::IsEnabled(
      omnibox::kOmniboxWebUISizeWebViewToPreferredHeight);
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady)) {
    config.defer_until_visual_state = base::Milliseconds(
        omnibox::kOmniboxWebUIDeferShowUntilVisualStateReadyTimeoutMs.Get());
  }
  return config;
}

// static
OmniboxPopupContentConfig OmniboxPopupContentConfig::ForFull() {
  OmniboxPopupContentConfig config;
  config.evict_on_hide =
      base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIEvictOnHide);
  config.detach_on_hide = base::FeatureList::IsEnabled(
      omnibox::kOmniboxFullWebUIDetachWebContentsOnHide);
  config.draw_shadow_in_webui = omnibox::ShouldDrawFullPopupShadowInWebUI();
  config.debounce_resize =
      base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIDebounceResize);
  config.height_workarounds =
      base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIHeightWorkarounds);
  config.size_webview_to_preferred_height = base::FeatureList::IsEnabled(
      omnibox::kOmniboxFullWebUISizeWebViewToPreferredHeight);
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxFullWebUIDeferShowUntilVisualStateReady)) {
    config.defer_until_visual_state = base::Milliseconds(
        omnibox::kOmniboxFullWebUIDeferShowUntilVisualStateReadyTimeoutMs
            .Get());
  }
  return config;
}

// static
OmniboxPopupContentConfig OmniboxPopupContentConfig::ForFullInLocationBar() {
  OmniboxPopupContentConfig config = ForFull();
  // Embedded in LocationBarView; the popped-out dropdown draws its own
  // shadow, never the host content.
  config.draw_shadow_in_webui = false;
  return config;
}

// static
OmniboxPopupContentConfig OmniboxPopupContentConfig::ForAim() {
  OmniboxPopupContentConfig config;
  config.evict_on_hide = omnibox::ShouldAimEvictOnHide();
  config.detach_on_hide =
      base::FeatureList::IsEnabled(omnibox::kOmniboxAimDetachWebContentsOnHide);
  config.draw_shadow_in_webui = omnibox::ShouldDrawAimShadowInWebUI();
  config.debounce_resize =
      base::FeatureList::IsEnabled(omnibox::kOmniboxAimDebounceResize);
  config.height_workarounds = omnibox::ShouldApplyAimHeightWorkarounds();
  config.size_webview_to_preferred_height = base::FeatureList::IsEnabled(
      omnibox::kOmniboxAimSizeWebViewToPreferredHeight);
  if (omnibox::ShouldDeferAimShowUntilVisualStateReady()) {
    config.defer_until_visual_state = base::Milliseconds(
        omnibox::kOmniboxAimDeferShowUntilVisualStateReadyTimeoutMs.Get());
  }
  return config;
}
