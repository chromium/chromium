// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

TabMetricsProvider::TabMetricsProvider(ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {}

TabMetricsProvider::~TabMetricsProvider() = default;

VerticalTabsState TabMetricsProvider::GetVerticalTabsState() {
  std::vector<Profile*> profiles = profile_manager_->GetLoadedProfiles();
  if (profiles.empty()) {
    return VerticalTabsState::kAllHorizontal;
  }

  size_t vertical_tab_profiles = 0;
  for (Profile* profile : profiles) {
    if (profile->GetPrefs() &&
        profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsEnabled)) {
      vertical_tab_profiles++;
    }
  }

  if (vertical_tab_profiles == 0) {
    return VerticalTabsState::kAllHorizontal;
  } else if (vertical_tab_profiles == profiles.size()) {
    return VerticalTabsState::kAllVertical;
  } else {
    return VerticalTabsState::kMixed;
  }
}

void TabMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  if (!tabs::IsVerticalTabsFeatureEnabled()) {
    return;
  }

  base::UmaHistogramEnumeration("Tabs.VerticalTabs.State",
                                GetVerticalTabsState());
}
