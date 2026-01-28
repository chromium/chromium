// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_search_feature.h"

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/variations/service/variations_service.h"

namespace {
static std::string GetCountryCode() {
  if (!g_browser_process || !g_browser_process->variations_service()) {
    return std::string();
  }
  std::string country_code =
      g_browser_process->variations_service()->GetStoredPermanentCountry();
  if (country_code.empty()) {
    country_code = g_browser_process->variations_service()->GetLatestCountry();
  }
  return country_code;
}
}  // namespace

namespace features {
bool HasTabSearchToolbarButton() {
  static const bool is_tab_search_moving = [] {
    if (GetCountryCode() == "us" &&
        base::FeatureList::IsEnabled(
            features::kLaunchedTabSearchToolbarButton)) {
      return true;
    }
    return base::FeatureList::IsEnabled(features::kTabstripComboButton) &&
           features::kTabSearchToolbarButton.Get();
  }();

  return is_tab_search_moving;
}
}  // namespace features
