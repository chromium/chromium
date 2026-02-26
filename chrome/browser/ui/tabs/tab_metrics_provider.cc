// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

TabMetricsProvider::TabMetricsProvider(ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {
  for (Profile* profile : profile_manager_->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
  profile_manager_->AddObserver(this);
}

TabMetricsProvider::~TabMetricsProvider() {
  profile_manager_->RemoveObserver(this);
}

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

void TabMetricsProvider::OnProfileAdded(Profile* profile) {
  if (!profile->IsRegularProfile()) {
    return;
  }

  UserEducationService* user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(profile);
  if (!user_education_service) {
    return;
  }

  user_education::UserEducationSessionManager& user_education_session_manager =
      user_education_service->user_education_session_manager();
  if (user_education_session_manager.GetNewSessionSinceStartup()) {
    OnUserEducationSessionStart(profile);
  }
  session_start_subscriptions_.push_back(
      user_education_session_manager.AddNewSessionCallback(
          base::BindRepeating(&TabMetricsProvider::OnUserEducationSessionStart,
                              base::Unretained(this), profile)));
}

void TabMetricsProvider::OnUserEducationSessionStart(Profile* profile) {
  base::UmaHistogramBoolean(
      "Tabs.VerticalTabs.EnabledAtSessionStart",
      profile->GetPrefs() &&
          profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsEnabled));
}
