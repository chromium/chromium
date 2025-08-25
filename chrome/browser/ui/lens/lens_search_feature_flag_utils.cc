// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/lens/lens_features.h"
#include "components/variations/service/variations_service.h"

namespace lens {

bool IsLensOverlayContextualSearchboxEnabled() {
  // If the feature is overridden (e.g. via server-side config or command-line),
  // use that state.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      (feature_list->IsFeatureOverridden(
           lens::features::kLensOverlayContextualSearchbox.name) ||
       feature_list->IsFeatureOverridden(
           lens::features::kLensOverlayContextualSearchboxForOmniboxSuggestions
               .name))) {
    // Important: If a server-side config applies to this client (i.e. after
    // accounting for its filters), but the client gets assigned to the default
    // group, they will still take this code path and receive the state
    // specified via BASE_FEATURE() above.
    return base::FeatureList::IsEnabled(
               lens::features::kLensOverlayContextualSearchbox) ||
           base::FeatureList::IsEnabled(
               lens::features::
                   kLensOverlayContextualSearchboxForOmniboxSuggestions);
  }

  // Safety check since this is a CP'd change.
  if (!g_browser_process) {
    DCHECK(g_browser_process) << "g_browser_process is null";
    return false;
  }

  // VariationsService and Features should exist.
  auto* variations_service = g_browser_process->variations_service();
  auto* features = g_browser_process->GetFeatures();

  // Safety check since this is a CP'd change.
  if(!variations_service || !features) {
    return false;
  }

  // Otherwise, enable it in the US to en-US locales via client-side code.
  return variations_service->GetStoredPermanentCountry() == "us" &&
         features->application_locale_storage() &&
         features->application_locale_storage()->Get() == "en-US";
}

}  // namespace lens
