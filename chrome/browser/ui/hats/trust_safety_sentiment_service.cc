// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include <map>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/safety_hub/card_data_helper.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/channel.h"

namespace {

base::TimeDelta GetMinTimeToPrompt() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2MinTimeToPrompt.Get()
             : features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
}

base::TimeDelta GetMaxTimeToPrompt() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2MaxTimeToPrompt.Get()
             : features::kTrustSafetySentimentSurveyMaxTimeToPrompt.Get();
}

base::TimeDelta GetMinSessionTime() {
  DCHECK(base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2));
  return features::kTrustSafetySentimentSurveyV2MinSessionTime.Get();
}

int GetRequiredNtpCount() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? base::RandInt(
                   features::kTrustSafetySentimentSurveyV2NtpVisitsMinRange
                       .Get(),
                   features::kTrustSafetySentimentSurveyV2NtpVisitsMaxRange
                       .Get())
             : base::RandInt(
                   features::kTrustSafetySentimentSurveyNtpVisitsMinRange.Get(),
                   features::kTrustSafetySentimentSurveyNtpVisitsMaxRange
                       .Get());
}

int GetMaxRequiredNtpCount() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2NtpVisitsMaxRange.Get()
             : features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get();
}

bool HasNonDefaultPrivacySetting(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  std::vector<std::string> prefs_to_check = {
      prefs::kSafeBrowsingEnabled,
      prefs::kSafeBrowsingEnhanced,
      prefs::kSafeBrowsingScoutReportingEnabled,
      prefs::kEnableDoNotTrack,
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      prefs::kCookieControlsMode,
  };

  bool has_non_default_pref = false;
  for (const auto& pref_name : prefs_to_check) {
    auto* pref = prefs->FindPreference(pref_name);
    if (!pref->IsDefaultValue() && pref->IsUserControlled()) {
      has_non_default_pref = true;
      break;
    }
  }

  // Users consenting to sync automatically enable UKM collection
  auto* ukm_pref = prefs->FindPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  auto* sync_consent_pref =
      prefs->FindPreference(prefs::kGoogleServicesConsentedToSync);

  bool has_non_default_ukm =
      ukm_pref->GetValue()->GetBool() !=
          sync_consent_pref->GetValue()->GetBool() &&
      (ukm_pref->IsUserControlled() || sync_consent_pref->IsUserControlled());

  // Check the default value for each user facing content setting. Note that
  // this will not include content setting exceptions set via permission
  // prompts, as they are site specific.
  bool has_non_default_content_setting = false;
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  for (auto content_setting_type :
       site_settings::GetVisiblePermissionCategories()) {
    content_settings::ProviderType content_setting_provider;
    auto current_value = map->GetDefaultContentSetting(
        content_setting_type, &content_setting_provider);
    auto content_setting_source =
        content_settings::GetSettingSourceFromProviderType(
            content_setting_provider);

    const bool user_controlled =
        content_setting_source == content_settings::SettingSource::kNone ||
        content_setting_source == content_settings::SettingSource::kUser;

    auto default_value = static_cast<ContentSetting>(
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_setting_type)
            ->initial_default_value()
            .GetInt());

    if (current_value != default_value && user_controlled) {
      has_non_default_content_setting = true;
      break;
    }
  }

  return has_non_default_pref || has_non_default_ukm ||
         has_non_default_content_setting;
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

// Returns true if the threat_type is not in the phishing, malware, unwanted
// software, or billing threat categories.
bool IsOtherSBInterstitialCategory(safe_browsing::SBThreatType threat_type) {
  switch (threat_type) {
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING:
      return false;
    default:
      return true;
  }
}

// Generates the Product Specific Data which accompanies survey results for the
// Password Protection UI product area.
std::map<std::string, bool> BuildProductSpecificDataForPasswordProtection(
    Profile* profile,
    PasswordProtectionUIType ui_type,
    PasswordProtectionUIAction action) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Enhanced protection enabled"] =
      safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs());
  product_specific_data["Is page info UI"] = false;
  product_specific_data["Is modal dialog UI"] = false;
  product_specific_data["Is interstitial UI"] = false;
  switch (ui_type) {
    case PasswordProtectionUIType::PAGE_INFO:
      product_specific_data["Is page info UI"] = true;
      break;
    case PasswordProtectionUIType::MODAL_DIALOG:
      product_specific_data["Is modal dialog UI"] = true;
      break;
    case PasswordProtectionUIType::INTERSTITIAL:
      product_specific_data["Is interstitial UI"] = true;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  product_specific_data["User completed password change"] = false;
  product_specific_data["User clicked change password"] = false;
  product_specific_data["User ignored warning"] = false;
  product_specific_data["User marked as legitimate"] = false;
  switch (action) {
    case PasswordProtectionUIAction::CHANGE_PASSWORD:
      product_specific_data["User clicked change password"] = true;
      break;
    case PasswordProtectionUIAction::IGNORE_WARNING:
    case PasswordProtectionUIAction::CLOSE:
      product_specific_data["User ignored warning"] = true;
      break;
    case PasswordProtectionUIAction::MARK_AS_LEGITIMATE:
      product_specific_data["User marked as legitimate"] = true;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return product_specific_data;
}

}  // namespace

TrustSafetySentimentService::TrustSafetySentimentService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  observed_profiles_.AddObservation(profile);

  // As this service is created lazily, there may already be a primary OTR
  // profile created for the main profile.
  if (auto* primary_otr =
          profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)) {
    observed_profiles_.AddObservation(primary_otr);
  }

  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
    performed_control_group_dice_roll_ = false;
  }
}

TrustSafetySentimentService::~TrustSafetySentimentService() {
  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  }
}

void TrustSafetySentimentService::OpenedNewTabPage() {
  // Explicit early exit for the common path, where the user has not performed
  // any of the trigger actions.
  if (pending_triggers_.size() == 0)
    return;

  // Reduce the NTPs to open count for all the active triggers.
  for (auto& area_trigger : pending_triggers_) {
    auto& trigger = area_trigger.second;
    if (trigger.remaining_ntps_to_open > 0)
      trigger.remaining_ntps_to_open--;
  }

  // Cleanup any triggers which are no longer relevant. This will be every
  // trigger which occurred more than the maximum prompt time ago, or the
  // trigger for the kIneligible area if it is no longer blocking
  // eligibility.
  std::erase_if(pending_triggers_,
                [](const std::pair<FeatureArea, PendingTrigger>& area_trigger) {
                  return base::Time::Now() - area_trigger.second.occurred_time >
                             GetMaxTimeToPrompt() ||
                         (area_trigger.first == FeatureArea::kIneligible &&
                          !ShouldBlockSurvey(area_trigger.second));
                });

  // This may have emptied the set of pending triggers.
  if (pending_triggers_.size() == 0)
    return;

  // A primary OTR profile (incognito) existing will prevent any surveys from
  // being shown.
  if (profile_->HasPrimaryOTRProfile())
    return;

  // Check if any of the triggers make the user not yet eligible to receive a
  // survey.
  for (const auto& area_trigger : pending_triggers_) {
    if (ShouldBlockSurvey(area_trigger.second))
      return;
  }

  // Choose a trigger at random to avoid any order biasing.
  auto winning_area_iterator = pending_triggers_.begin();
  std::advance(winning_area_iterator,
               base::RandInt(0, pending_triggers_.size() - 1));

  // The winning feature area should never be kIneligible, as this will
  // have either been removed above, or blocked showing any survey.
  DCHECK(winning_area_iterator->first != FeatureArea::kIneligible);

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  // A null HaTS service should have prevented this service from being created.
  DCHECK(hats_service);
  hats_service->LaunchSurvey(
      GetHatsTriggerForFeatureArea(winning_area_iterator->first),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      winning_area_iterator->second.product_specific_data);
  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.SurveyRequested",
                                winning_area_iterator->first);
  pending_triggers_.clear();
}

void TrustSafetySentimentService::InteractedWithPrivacySettings(
    content::WebContents* web_contents) {
  // Only observe one instance settings at a time. This ignores both multiple
  // instances of settings, and repeated interactions with settings. This
  // reduces the chance that a user is eligible for a survey, but is much
  // simpler. As interactions with settings (visiting password manager and using
  // the privacy card) can occur independently, there is also little risk of
  // starving one interaction.
  if (settings_watcher_)
    return;

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyPrivacySettingsTime.Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(),
                     FeatureArea::kPrivacySettings,
                     GetPrivacySettingsProductSpecificData(
                         profile_, /*ran_safety_check=*/false)),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::RanSafetyCheck() {
  // Since we have logic to block a trigger for an incorrect version, we can
  // call both of these and only the appropriate trigger and probability will be
  // recorded.
  TriggerOccurred(FeatureArea::kSafetyCheck, {});
  TriggerOccurred(FeatureArea::kPrivacySettings,
                  GetPrivacySettingsProductSpecificData(
                      profile_, /*ran_safety_check=*/true));
}

void TrustSafetySentimentService::PageInfoOpened() {
  // Only one Page Info should ever be open.
  DCHECK(!page_info_state_);
  page_info_state_ = std::make_unique<PageInfoState>();
}

void TrustSafetySentimentService::InteractedWithPageInfo() {
  DCHECK(page_info_state_);
  page_info_state_->interacted = true;
}

void TrustSafetySentimentService::PageInfoClosed() {
  if (!page_info_state_) {
    return;
  }

  base::TimeDelta threshold =
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
          ? features::kTrustSafetySentimentSurveyV2TrustedSurfaceTime.Get()
          : features::kTrustSafetySentimentSurveyTrustedSurfaceTime.Get();
  // Record a trigger if either the user had page info open for the required
  // time, or if they interacted with it.
  if (base::Time::Now() - page_info_state_->opened_time >= threshold ||
      page_info_state_->interacted) {
    TriggerOccurred(
        FeatureArea::kTrustedSurface,
        {{"Interacted with Page Info", page_info_state_->interacted}});
  }

  page_info_state_.reset();
}

void TrustSafetySentimentService::SavedPassword() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", true}});
}

void TrustSafetySentimentService::OpenedPasswordManager(
    content::WebContents* web_contents) {
  if (settings_watcher_)
    return;

  std::map<std::string, bool> product_specific_data = {
      {"Saved password", false}};

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyTransactionsPasswordManagerTime
          .Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(), FeatureArea::kTransactions,
                     product_specific_data),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::SavedCard() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", false}});
}

void TrustSafetySentimentService::RanPasswordCheck() {
  TriggerOccurred(FeatureArea::kPasswordCheck, {});
}

void TrustSafetySentimentService::ClearedBrowsingData(
    browsing_data::BrowsingDataType datatype) {
  // We are only interested in history, downloads, and autofill.
  switch (datatype) {
    case (browsing_data::BrowsingDataType::HISTORY):
    case (browsing_data::BrowsingDataType::DOWNLOADS):
    case (browsing_data::BrowsingDataType::FORM_DATA):
      break;
    default:
      return;
  }
  return TriggerOccurred(
      FeatureArea::kBrowsingData,
      {{"Deleted history",
        datatype == browsing_data::BrowsingDataType::HISTORY},
       {"Deleted downloads",
        datatype == browsing_data::BrowsingDataType::DOWNLOADS},
       {"Deleted autofill form data",
        datatype == browsing_data::BrowsingDataType::FORM_DATA}});
  ;
}

void TrustSafetySentimentService::FinishedPrivacyGuide() {
  TriggerOccurred(FeatureArea::kPrivacyGuide, {});
}

void TrustSafetySentimentService::InteractedWithPrivacySandbox4(
    FeatureArea feature_area) {
  TriggerOccurred(feature_area, {});
}

void TrustSafetySentimentService::InteractedWithSafeBrowsingInterstitial(
    bool did_proceed,
    safe_browsing::SBThreatType threat_type) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["User proceeded past interstitial"] = did_proceed;
  product_specific_data["Enhanced protection enabled"] =
      safe_browsing::IsEnhancedProtectionEnabled(*profile_->GetPrefs());
  product_specific_data["Threat is phishing"] =
      threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING ||
      threat_type ==
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
  product_specific_data["Threat is malware"] =
      threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
  product_specific_data["Threat is unwanted software"] =
      threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
  product_specific_data["Threat is billing"] =
      threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
  DCHECK(!IsOtherSBInterstitialCategory(threat_type));
  TriggerOccurred(FeatureArea::kSafeBrowsingInterstitial,
                  product_specific_data);
}

void TrustSafetySentimentService::InteractedWithDownloadWarningUI(
    DownloadItemWarningData::WarningSurface surface,
    DownloadItemWarningData::WarningAction action) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Enhanced protection enabled"] =
      safe_browsing::IsEnhancedProtectionEnabled(*profile_->GetPrefs());
  product_specific_data["Is mainpage UI"] = false;
  product_specific_data["Is downloads page UI"] = false;
  product_specific_data["Is download prompt UI"] = false;
  product_specific_data["User proceeded past warning"] = false;
  product_specific_data["Is subpage UI"] = false;
  switch (surface) {
    case DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE:
      product_specific_data["Is mainpage UI"] = true;
      break;
    case DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE:
      product_specific_data["Is subpage UI"] = true;
      break;
    case DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE:
      product_specific_data["Is downloads page UI"] = true;
      break;
    case DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT:
      product_specific_data["Is download prompt UI"] = true;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  switch (action) {
    case DownloadItemWarningData::WarningAction::PROCEED:
      product_specific_data["User proceeded past warning"] = true;
      break;
    case DownloadItemWarningData::WarningAction::DISCARD:
      product_specific_data["User proceeded past warning"] = false;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  TriggerOccurred(FeatureArea::kDownloadWarningUI, product_specific_data);
}

void TrustSafetySentimentService::ProtectResetOrCheckPasswordClicked(
    PasswordProtectionUIType ui_type) {
  // Only one Phished Password Change should ever be open.
  DCHECK(!phished_password_change_state_);
  phished_password_change_state_ =
      std::make_unique<PhishedPasswordChangeState>();
  phished_password_change_state_->ui_type_ = ui_type;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TrustSafetySentimentService::MaybeTriggerPasswordProtectionSurvey,
          weak_ptr_factory_.GetWeakPtr(), ui_type,
          PasswordProtectionUIAction::CHANGE_PASSWORD),
      kPasswordChangeInactivity);
}

void TrustSafetySentimentService::PhishedPasswordUpdateNotClicked(
    PasswordProtectionUIType ui_type,
    PasswordProtectionUIAction action) {
  DCHECK(action != PasswordProtectionUIAction::CHANGE_PASSWORD);
  MaybeTriggerPasswordProtectionSurvey(ui_type, action);
}

void TrustSafetySentimentService::PhishedPasswordUpdateFinished() {
  if (!phished_password_change_state_) {
    return;
  }
  phished_password_change_state_->finished_action = true;
  MaybeTriggerPasswordProtectionSurvey(
      phished_password_change_state_->ui_type_,
      PasswordProtectionUIAction::CHANGE_PASSWORD);
}

void TrustSafetySentimentService::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  // Only interested in the primary OTR profile i.e. the one used for incognito
  // browsing. Non-primary OTR profiles are often used as implementation details
  // of other features, and are not inherintly relevant to Trust & Safety.
  if (off_the_record->GetOTRProfileID() == Profile::OTRProfileID::PrimaryID())
    observed_profiles_.AddObservation(off_the_record);
}

void TrustSafetySentimentService::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);

  if (profile->IsOffTheRecord()) {
    // Closing the incognito profile, which is the only OTR profie observed by
    // this class, is an ileligible action.
    PerformedIneligibleAction();
  }
}

void TrustSafetySentimentService::OnSessionEnded(base::TimeDelta session_length,
                                                 base::TimeTicks session_end) {
  DCHECK(base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2));
  // Check if the user is eligible for the control group.
  if (!performed_control_group_dice_roll_ &&
      session_length > GetMinSessionTime()) {
    performed_control_group_dice_roll_ = true;
    TriggerOccurred(FeatureArea::kControlGroup, {});
  }
}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const std::map<std::string, bool>& product_specific_data,
    int remaining_ntps_to_open)
    : product_specific_data(product_specific_data),
      remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    int remaining_ntps_to_open)
    : remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::~PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const PendingTrigger& other) = default;

TrustSafetySentimentService::SettingsWatcher::SettingsWatcher(
    content::WebContents* web_contents,
    base::TimeDelta required_open_time,
    base::OnceCallback<void()> success_callback,
    base::OnceCallback<void()> complete_callback)
    : web_contents_(web_contents),
      success_callback_(std::move(success_callback)),
      complete_callback_(std::move(complete_callback)) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TrustSafetySentimentService::SettingsWatcher::TimerComplete,
          weak_ptr_factory_.GetWeakPtr()),
      required_open_time);
  Observe(web_contents);
}

TrustSafetySentimentService::SettingsWatcher::~SettingsWatcher() = default;

void TrustSafetySentimentService::SettingsWatcher::WebContentsDestroyed() {
  std::move(complete_callback_).Run();
}

void TrustSafetySentimentService::SettingsWatcher::TimerComplete() {
  const bool stayed_on_settings =
      web_contents_ &&
      web_contents_->GetVisibility() == content::Visibility::VISIBLE &&
      web_contents_->GetLastCommittedURL().host_piece() ==
          chrome::kChromeUISettingsHost;
  if (stayed_on_settings)
    std::move(success_callback_).Run();

  std::move(complete_callback_).Run();
}

TrustSafetySentimentService::PageInfoState::PageInfoState()
    : opened_time(base::Time::Now()) {}

TrustSafetySentimentService::PhishedPasswordChangeState::
    PhishedPasswordChangeState()
    : password_change_click_ts_(base::Time::Now()),
      ui_type_(PasswordProtectionUIType::NOT_USED) {}

void TrustSafetySentimentService::SettingsWatcherComplete() {
  settings_watcher_.reset();
}

void TrustSafetySentimentService::TriggerOccurred(
    FeatureArea feature_area,
    const std::map<std::string, bool>& product_specific_data) {
  // Log histogram that verifies infrastructure works as intended.
  base::UmaHistogramEnumeration(
      "Feedback.TrustSafetySentiment.CallTriggerOccurred", feature_area);
  if (!ProbabilityCheck(feature_area))
    return;

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                feature_area);

  // This will overwrite any previous trigger for this feature area. We are
  // only interested in the most recent trigger, so this is acceptable.
  pending_triggers_[feature_area] =
      PendingTrigger(product_specific_data, GetRequiredNtpCount());
}

void TrustSafetySentimentService::PerformedIneligibleAction() {
  pending_triggers_[FeatureArea::kIneligible] =
      PendingTrigger(GetMaxRequiredNtpCount());

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                FeatureArea::kIneligible);
}

/*static*/ bool TrustSafetySentimentService::ShouldBlockSurvey(
    const PendingTrigger& trigger) {
  return base::Time::Now() - trigger.occurred_time < GetMinTimeToPrompt() ||
         trigger.remaining_ntps_to_open > 0;
}

// Checks inactivity delay and finished_action (change psd field to true)
void TrustSafetySentimentService::MaybeTriggerPasswordProtectionSurvey(
    PasswordProtectionUIType ui_type,
    PasswordProtectionUIAction action) {
  DCHECK(ui_type != PasswordProtectionUIType::NOT_USED);
  std::map<std::string, bool> product_specific_data =
      BuildProductSpecificDataForPasswordProtection(profile_, ui_type, action);
  if (action == PasswordProtectionUIAction::CHANGE_PASSWORD) {
    if (!phished_password_change_state_) {
      return;
    }
    if (!phished_password_change_state_->finished_action &&
        base::Time::Now() -
                phished_password_change_state_->password_change_click_ts_ <
            kPasswordChangeInactivity) {
      return;
    }
    if (phished_password_change_state_->finished_action) {
      product_specific_data["User completed password change"] = true;
    }
    phished_password_change_state_.reset();
  }
  TriggerOccurred(FeatureArea::kPasswordProtectionUI, product_specific_data);
}

void TrustSafetySentimentService::TriggerSafetyHubSurvey(
    TrustSafetySentimentService::FeatureArea feature_area,
    std::map<std::string, bool> product_specific_data) {
  if (!base::FeatureList::IsEnabled(
          features::kSafetyHubTrustSafetySentimentSurvey)) {
    return;
  }
  // Delay the trigger to determine whether the user interacted with Safety Hub
  // soon after the trigger occurred.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(), feature_area,
                     product_specific_data),
      kSafetyHubSurveyDelay);
}

// static
bool TrustSafetySentimentService::VersionCheck(FeatureArea feature_area) {
  bool isV2 =
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2);
  switch (feature_area) {
    // Version 1 only
    case (FeatureArea::kPrivacySettings):
    case (FeatureArea::kTransactions):
      return isV2 == false;
    // Version 2 only
    case (FeatureArea::kSafetyCheck):
    case (FeatureArea::kSafetyHubInteracted):
    case (FeatureArea::kSafetyHubNotification):
    case (FeatureArea::kPasswordCheck):
    case (FeatureArea::kBrowsingData):
    case (FeatureArea::kPrivacyGuide):
    case (FeatureArea::kControlGroup):
    case (FeatureArea::kSafeBrowsingInterstitial):
    case (FeatureArea::kDownloadWarningUI):
    case (FeatureArea::kPasswordProtectionUI):
      return isV2 == true;
    // Both Versions
    case (FeatureArea::kTrustedSurface):
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
    case (FeatureArea::kPrivacySandbox4NoticeOk):
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return true;
    // None
    case (FeatureArea::kIneligible):
      return false;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// static
std::string TrustSafetySentimentService::GetHatsTriggerForFeatureArea(
    FeatureArea feature_area) {
  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    switch (feature_area) {
      case (FeatureArea::kTrustedSurface):
        return kHatsSurveyTriggerTrustSafetyV2TrustedSurface;
      case (FeatureArea::kSafetyCheck):
        return kHatsSurveyTriggerTrustSafetyV2SafetyCheck;
      case (FeatureArea::kSafetyHubInteracted):
        return kHatsSurveyTriggerTrustSafetyV2SafetyHubInteraction;
      case (FeatureArea::kSafetyHubNotification):
        return kHatsSurveyTriggerTrustSafetyV2SafetyHubNotification;
      case (FeatureArea::kPasswordCheck):
        return kHatsSurveyTriggerTrustSafetyV2PasswordCheck;
      case (FeatureArea::kBrowsingData):
        return kHatsSurveyTriggerTrustSafetyV2BrowsingData;
      case (FeatureArea::kPrivacyGuide):
        return kHatsSurveyTriggerTrustSafetyV2PrivacyGuide;
      case (FeatureArea::kControlGroup):
        return kHatsSurveyTriggerTrustSafetyV2ControlGroup;
      case (FeatureArea::kPrivacySandbox4ConsentAccept):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept;
      case (FeatureArea::kPrivacySandbox4ConsentDecline):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline;
      case (FeatureArea::kPrivacySandbox4NoticeOk):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk;
      case (FeatureArea::kPrivacySandbox4NoticeSettings):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings;
      case (FeatureArea::kSafeBrowsingInterstitial):
        return kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial;
      case (FeatureArea::kDownloadWarningUI):
        return kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI;
      case (FeatureArea::kPasswordProtectionUI):
        return kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI;
      default:
        NOTREACHED_IN_MIGRATION();
        return "";
    }
  }
  switch (feature_area) {
    case (FeatureArea::kPrivacySettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySettings;
    case (FeatureArea::kTrustedSurface):
      return kHatsSurveyTriggerTrustSafetyTrustedSurface;
    case (FeatureArea::kTransactions):
      return kHatsSurveyTriggerTrustSafetyTransactions;
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept;
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline;
    case (FeatureArea::kPrivacySandbox4NoticeOk):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk;
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings;
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

// static
bool TrustSafetySentimentService::ProbabilityCheck(FeatureArea feature_area) {
  if (!TrustSafetySentimentService::VersionCheck(feature_area)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    switch (feature_area) {
      case (FeatureArea::kTrustedSurface):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2TrustedSurfaceProbability
                   .Get();
      case (FeatureArea::kSafetyCheck):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2SafetyCheckProbability
                   .Get();
      case (FeatureArea::kSafetyHubInteracted):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2SafetyHubInteractionProbability
                       .Get();
      case (FeatureArea::kSafetyHubNotification):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2SafetyHubNotificationProbability
                       .Get();
      case (FeatureArea::kPasswordCheck):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2PasswordCheckProbability
                   .Get();
      case (FeatureArea::kBrowsingData):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2BrowsingDataProbability
                   .Get();
      case (FeatureArea::kPrivacyGuide):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2PrivacyGuideProbability
                   .Get();
      case (FeatureArea::kControlGroup):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2ControlGroupProbability
                   .Get();
      case (FeatureArea::kPrivacySandbox4ConsentAccept):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4ConsentDecline):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4NoticeOk):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4NoticeSettings):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsProbability
                       .Get();
      case (FeatureArea::kSafeBrowsingInterstitial):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialProbability
                       .Get();
      case (FeatureArea::kDownloadWarningUI):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2DownloadWarningUIProbability
                       .Get();
      case (FeatureArea::kPasswordProtectionUI):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PasswordProtectionUIProbability
                       .Get();
      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }

  switch (feature_area) {
    case (FeatureArea::kPrivacySettings):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyPrivacySettingsProbability
                 .Get();
    case (FeatureArea::kTrustedSurface):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTrustedSurfaceProbability
                 .Get();
    case (FeatureArea::kTransactions):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTransactionsProbability.Get();
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4NoticeOk):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsProbability
                     .Get();
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}
