// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/rand_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/unified_consent/pref_names.h"

namespace {

base::TimeDelta GetMinTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
}

base::TimeDelta GetMaxTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMaxTimeToPrompt.Get();
}

int GetRequiredNtpCount() {
  return base::RandInt(
      features::kTrustSafetySentimentSurveyNtpVisitsMinRange.Get(),
      features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get());
}

std::string GetHatsTriggerForFeatureArea(
    TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySettings;
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return kHatsSurveyTriggerTrustSafetyTrustedSurface;
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return kHatsSurveyTriggerTrustSafetyTransactions;
  }
  NOTREACHED();
  return "";
}

bool ProbabilityCheck(TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyPrivacySettingsProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTrustedSurfaceProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTransactionsProbability.Get();
  }
  NOTREACHED();
  return false;
}

bool HasNonDefaultPrivacySetting(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  // Check for the most relevant set of privacy preferences.
  const bool has_non_default_pref =
      !prefs->FindPreference(prefs::kSafeBrowsingEnabled)->IsDefaultValue() ||
      !prefs->FindPreference(prefs::kSafeBrowsingEnhanced)->IsDefaultValue() ||
      !prefs->FindPreference(prefs::kSafeBrowsingScoutReportingEnabled)
           ->IsDefaultValue() ||
      !prefs->FindPreference(prefs::kEnableDoNotTrack)->IsDefaultValue() ||
      !prefs
           ->FindPreference(
               password_manager::prefs::kPasswordLeakDetectionEnabled)
           ->IsDefaultValue() ||
      !prefs->FindPreference(prefs::kCookieControlsMode)->IsDefaultValue() ||
      // Users consenting to sync automatically enable UKM collection
      (prefs->GetBoolean(
           unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled) !=
       prefs->GetBoolean(prefs::kGoogleServicesConsentedToSync));

  // Check the default value for each user facing content setting. Note that
  // this will not include content setting exceptions set via permission
  // prompts, as they are site specific.
  bool has_non_default_content_setting = false;
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  for (auto content_setting_type :
       site_settings::GetVisiblePermissionCategories()) {
    auto current_value = map->GetDefaultContentSetting(content_setting_type,
                                                       /*provider_id=*/nullptr);
    auto default_value = static_cast<ContentSetting>(
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_setting_type)
            ->initial_default_value()
            ->GetInt());

    if (current_value != default_value) {
      has_non_default_content_setting = true;
      break;
    }
  }

  return has_non_default_pref || has_non_default_content_setting;
}

// Generates the Product Specific Data which accompanies survey results for the
// Privacy Settings product area. This includes whether the user is receiving
// the survey because they ran safety check, and whether they have any
// non-default core privacy settings.
std::map<std::string, bool> GetPrivacySettingsProductSpecificData(
    Profile* profile,
    bool ran_safety_check) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Non default setting"] =
      HasNonDefaultPrivacySetting(profile);
  product_specific_data["Ran safety check"] = ran_safety_check;
  return product_specific_data;
}

}  // namespace

TrustSafetySentimentService::TrustSafetySentimentService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}

TrustSafetySentimentService::~TrustSafetySentimentService() = default;

void TrustSafetySentimentService::OpenedNewTabPage() {
  // Explicit early exit for the common path, where the user has not performed
  // any of the trigger actions.
  if (pending_triggers_.size() == 0)
    return;

  // Remove any triggers which occurred more than the maximum time ago.
  base::EraseIf(pending_triggers_,
                [](const std::pair<FeatureArea, PendingTrigger>& area_trigger) {
                  return base::Time::Now() - area_trigger.second.occurred_time >
                         GetMaxTimeToPrompt();
                });

  // This may have emptied the set of pending triggers.
  if (pending_triggers_.size() == 0)
    return;

  // Reduce the NTPs to open count for all the active triggers.
  for (auto& area_trigger : pending_triggers_) {
    auto& trigger = area_trigger.second;
    if (trigger.remaining_ntps_to_open > 0)
      trigger.remaining_ntps_to_open--;
  }

  // Any trigger being too recent, or not having the required number of NTP
  // opens, will prevent a survey from being shown.
  auto now = base::Time::Now();
  for (const auto& area_trigger : pending_triggers_) {
    const auto& trigger = area_trigger.second;
    if (now - trigger.occurred_time < GetMinTimeToPrompt() ||
        trigger.remaining_ntps_to_open > 0) {
      return;
    }
  }

  // Choose a trigger at random to avoid any order biasing.
  auto winning_area_iterator = pending_triggers_.begin();
  std::advance(winning_area_iterator,
               base::RandInt(0, pending_triggers_.size() - 1));

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  // A null HaTS service should have prevented this service from being created.
  DCHECK(hats_service);
  hats_service->LaunchSurvey(
      GetHatsTriggerForFeatureArea(winning_area_iterator->first),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      winning_area_iterator->second.product_specific_data);

  pending_triggers_.clear();
}

void TrustSafetySentimentService::InteractedWithPrivacySettings(
    content::WebContents* web_contents) {
  // Only observe one instance of the settings page at a time, simply ignoring
  // repeated settings events. This reduces the likelihood the user will be
  // recorded as staying on settings, but is much simpler.
  if (settings_watcher_)
    return;

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyPrivacySettingsTime.Get(),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::RanSafetyCheck() {
  TriggerOccurred(FeatureArea::kPrivacySettings,
                  GetPrivacySettingsProductSpecificData(
                      profile_, /*ran_safety_check=*/true));
}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const std::map<std::string, bool>& product_specific_data,
    int remaining_ntps_to_open)
    : product_specific_data(product_specific_data),
      remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::~PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const PendingTrigger& other) = default;

TrustSafetySentimentService::SettingsWatcher::SettingsWatcher(
    content::WebContents* web_contents,
    base::TimeDelta required_open_time,
    base::OnceCallback<void(bool)> complete_callback)
    : web_contents_(web_contents),
      complete_callback_(std::move(complete_callback)) {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TrustSafetySentimentService::SettingsWatcher::TimerComplete,
          weak_ptr_factory_.GetWeakPtr()),
      required_open_time);
  Observe(web_contents);
}

TrustSafetySentimentService::SettingsWatcher::~SettingsWatcher() = default;

void TrustSafetySentimentService::SettingsWatcher::WebContentsDestroyed() {
  std::move(complete_callback_).Run(/*stayed_on_settings*/ false);
}

void TrustSafetySentimentService::SettingsWatcher::TimerComplete() {
  const bool stayed_on_settings =
      web_contents_ &&
      web_contents_->GetVisibility() == content::Visibility::VISIBLE &&
      web_contents_->GetLastCommittedURL().host_piece() ==
          chrome::kChromeUISettingsHost;
  std::move(complete_callback_).Run(stayed_on_settings);
}

void TrustSafetySentimentService::SettingsWatcherComplete(
    bool stayed_on_settings) {
  settings_watcher_.reset();
  if (stayed_on_settings) {
    TriggerOccurred(FeatureArea::kPrivacySettings,
                    GetPrivacySettingsProductSpecificData(
                        profile_, /*ran_safety_check=*/false));
  }
}

void TrustSafetySentimentService::TriggerOccurred(
    FeatureArea feature_area,
    const std::map<std::string, bool>& product_specific_data) {
  if (!ProbabilityCheck(feature_area))
    return;

  // This will overwrite any previous trigger for this feature area. We are
  // only interested in the most recent trigger, so this is acceptable.
  pending_triggers_[feature_area] =
      PendingTrigger(product_specific_data, GetRequiredNtpCount());
}
