// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetMultiDeviceOptedInSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_FORGET,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kForgetPhone},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_FORGET_ALT1,
         SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kMessages,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kMessagesOnOff},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES_ALT1,
         SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kMultiDeviceFeatures},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_ALT1, SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_SMART_LOCK,
        mojom::kSmartLockSubpagePath,
        mojom::SearchResultIcon::kLock,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kSmartLockOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>& GetSmartLockOptionsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_SMART_LOCK_OPTIONS,
        mojom::kSmartLockSubpagePath,
        mojom::SearchResultIcon::kLock,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kSmartLockUnlockOrSignIn}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInPhoneHubSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubOnOff}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubNotificationsOnOff}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_NOTIFICATION_BADGE,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubNotificationBadgeOnOff}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubTaskContinuationOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInWifiSyncSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_WIFI_SYNC,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kWifi,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kWifiSyncOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>& GetMultiDeviceOptedOutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    // Special-case: The "set up" search tag also includes the names of the
    // multi-device features as a way to increase discoverability of these
    // features.
    SearchConcept set_up_concept{
        IDS_OS_SETTINGS_TAG_MULTIDEVICE_SET_UP,
        mojom::kMultiDeviceSectionPath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kSetUpMultiDevice},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE,
         IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES,
         IDS_OS_SETTINGS_TAG_MULTIDEVICE_SMART_LOCK, SearchConcept::kAltTagEnd},
    };

    // Include the following features in alternate message IDs if they are
    // enabled and the alt tag limit has not been reached: Phone Hub, Instant
    // Tethering and Wifi Sync.
    int alt_tag_index = 3;

    if (features::IsPhoneHubEnabled()) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB;
      alt_tag_index++;
    }
    if (base::FeatureList::IsEnabled(features::kInstantTethering)) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_INSTANT_TETHERING;
      alt_tag_index++;
    }

    // TODO(cvandermerwe): Update 5 alt tag limit to 6 and remove condition
    if (alt_tag_index < 5 && features::IsWifiSyncAndroidEnabled()) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_MULTIDEVICE_WIFI_SYNC;
      alt_tag_index++;
    }

    if (alt_tag_index < 5) {
      set_up_concept.alt_tag_ids[alt_tag_index] = SearchConcept::kAltTagEnd;
    }

    return std::vector<SearchConcept>{set_up_concept};
  }());
  return *tags;
}

const std::vector<SearchConcept>& GetNearbyShareOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MULTIDEVICE_NEARBY_SHARE,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kNearbyShare}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_OFF,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareOnOff},
       {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetNearbyShareOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_ON,
       mojom::kMultiDeviceSectionPath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareOnOff},
       {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_ON_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

void AddEasyUnlockStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"easyUnlockSectionTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"easyUnlockUnlockDeviceOnly",
       IDS_SETTINGS_EASY_UNLOCK_UNLOCK_DEVICE_ONLY},
      {"easyUnlockUnlockDeviceAndAllowSignin",
       IDS_SETTINGS_EASY_UNLOCK_UNLOCK_DEVICE_AND_ALLOW_SIGNIN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

bool IsOptedIn(multidevice_setup::mojom::HostStatus host_status) {
  return host_status ==
             multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified ||
         host_status == multidevice_setup::mojom::HostStatus::kHostVerified;
}

}  // namespace

MultiDeviceSection::MultiDeviceSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::PhoneHubManager* phone_hub_manager,
    android_sms::AndroidSmsService* android_sms_service,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      multidevice_setup_client_(multidevice_setup_client),
      phone_hub_manager_(phone_hub_manager),
      android_sms_service_(android_sms_service),
      pref_service_(pref_service) {
  if (base::FeatureList::IsEnabled(::features::kNearbySharing)) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        ::prefs::kNearbySharingEnabledPrefName,
        base::BindRepeating(&MultiDeviceSection::OnNearbySharingEnabledChanged,
                            base::Unretained(this)));
    OnNearbySharingEnabledChanged();
  }

  // Note: |multidevice_setup_client_| is null when multi-device features are
  // prohibited by policy.
  if (!multidevice_setup_client_)
    return;

  multidevice_setup_client_->AddObserver(this);
  OnHostStatusChanged(multidevice_setup_client_->GetHostStatus());
  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
}

MultiDeviceSection::~MultiDeviceSection() {
  if (multidevice_setup_client_)
    multidevice_setup_client_->RemoveObserver(this);
}

void MultiDeviceSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"multidevicePageTitle", IDS_SETTINGS_MULTIDEVICE},
      {"multideviceSetupButton", IDS_SETTINGS_MULTIDEVICE_SETUP_BUTTON},
      {"multideviceVerifyButton", IDS_SETTINGS_MULTIDEVICE_VERIFY_BUTTON},
      {"multideviceSetupItemHeading",
       IDS_SETTINGS_MULTIDEVICE_SETUP_ITEM_HEADING},
      {"multideviceEnabled", IDS_SETTINGS_MULTIDEVICE_ENABLED},
      {"multideviceDisabled", IDS_SETTINGS_MULTIDEVICE_DISABLED},
      {"multideviceSmartLockItemTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"multidevicePhoneHubItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_SECTION_TITLE},
      {"multidevicePhoneHubNotificationsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS_SECTION_TITLE},
      {"multidevicePhoneHubNotificationBadgeItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATION_BADGE_SECTION_TITLE},
      {"multidevicePhoneHubTaskContinuationItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_SECTION_TITLE},
      {"multidevicePhoneHubNotificationBadgeItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATION_BADGE_SUMMARY},
      {"multidevicePhoneHubTaskContinuationItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_SUMMARY},
      {"multideviceWifiSyncItemTitle",
       IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_SECTION_TITLE},
      {"multideviceNotificationAccessSetupAckTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_ACK_TITLE},
      {"multideviceNotificationAccessSetupConnectingTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_CONNECTING_TITLE},
      {"multideviceNotificationAccessSetupInstructions",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_INSTRUCTIONS},
      {"multideviceNotificationAccessSetupCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COMPLETED_TITLE},
      {"multideviceNotificationAccessSetupCompletedSummary",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COMPLETED_SUMMARY},
      {"multideviceInstantTetheringItemTitle",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING},
      {"multideviceInstantTetheringItemSummary",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING_SUMMARY},
      {"multideviceAndroidMessagesItemTitle",
       IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES},
      {"multideviceForgetDevice", IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE},
      {"multideviceSmartLockOptions",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"multideviceForgetDeviceDisconnect",
       IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_DISCONNECT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "multideviceAllowedByPolicy",
      chromeos::multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          profile()->GetPrefs()));

  html_source->AddString(
      "multideviceForgetDeviceSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_EXPLANATION));
  html_source->AddString(
      "multideviceForgetDeviceDialogMessage",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_DEVICE_DIALOG_MESSAGE));
  html_source->AddString(
      "multideviceVerificationText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_VERIFICATION_TEXT,
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceSetupSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SETUP_SUMMARY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceNoHostText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_NO_ELIGIBLE_HOSTS,
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceAndroidMessagesItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES_SUMMARY,
          ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              multidevice_setup::GetBoardSpecificMessagesLearnMoreUrl()
                  .spec())));
  html_source->AddString(
      "multideviceSmartLockItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SMART_LOCK_SUMMARY,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kEasyUnlockLearnMoreUrl)));
  html_source->AddString(
      "multidevicePhoneHubItemSummary",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_SUMMARY,
                                 ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "multidevicePhoneHubNotificationsItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS_SUMMARY,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "multideviceWifiSyncItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_SUMMARY,
          // TODO(cvandermerwe) Replace with WifiSyncLearnMoreUrl once created
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));

  AddEasyUnlockStrings(html_source);
  ::settings::AddNearbyShareData(html_source);
  RegisterNearbySharedStrings(html_source);
}

void MultiDeviceSection::AddHandlers(content::WebUI* web_ui) {
  // No handlers in guest mode.
  if (profile()->IsGuestSession())
    return;

  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::MultideviceHandler>(
          pref_service_, multidevice_setup_client_,
          phone_hub_manager_
              ? phone_hub_manager_->GetNotificationAccessManager()
              : nullptr,
          android_sms_service_
              ? android_sms_service_->android_sms_pairing_state_tracker()
              : nullptr,
          android_sms_service_ ? android_sms_service_->android_sms_app_manager()
                               : nullptr));
}

int MultiDeviceSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_MULTIDEVICE;
}

mojom::Section MultiDeviceSection::GetSection() const {
  return mojom::Section::kMultiDevice;
}

mojom::SearchResultIcon MultiDeviceSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPhone;
}

std::string MultiDeviceSection::GetSectionPath() const {
  return mojom::kMultiDeviceSectionPath;
}

bool MultiDeviceSection::LogMetric(mojom::Setting setting,
                                   base::Value& value) const {
  // Unimplemented.
  return false;
}

void MultiDeviceSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSetUpMultiDevice);
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifyMultiDeviceSetup);

  // MultiDevice features.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_MULTIDEVICE, mojom::Subpage::kMultiDeviceFeatures,
      mojom::SearchResultIcon::kPhone, mojom::SearchResultDefaultRank::kMedium,
      mojom::kMultiDeviceFeaturesSubpagePath);
  static constexpr mojom::Setting kMultiDeviceFeaturesSettings[] = {
      mojom::Setting::kMultiDeviceOnOff,
      mojom::Setting::kMessagesSetUp,
      mojom::Setting::kMessagesOnOff,
      mojom::Setting::kForgetPhone,
      mojom::Setting::kPhoneHubOnOff,
      mojom::Setting::kPhoneHubNotificationsOnOff,
      mojom::Setting::kPhoneHubNotificationBadgeOnOff,
      mojom::Setting::kPhoneHubTaskContinuationOnOff,
      mojom::Setting::kWifiSyncOnOff,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kMultiDeviceFeatures,
                            kMultiDeviceFeaturesSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kMultiDeviceOnOff);
  // Note: Instant Tethering is part of the Network section, but it has an
  // alternate setting within the MultiDevice section.
  generator->RegisterNestedAltSetting(mojom::Setting::kInstantTetheringOnOff,
                                      mojom::Subpage::kMultiDeviceFeatures);

  // Smart Lock.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE, mojom::Subpage::kSmartLock,
      mojom::Subpage::kMultiDeviceFeatures, mojom::SearchResultIcon::kLock,
      mojom::SearchResultDefaultRank::kMedium, mojom::kSmartLockSubpagePath);
  static constexpr mojom::Setting kSmartLockSettings[] = {
      mojom::Setting::kSmartLockOnOff,
      mojom::Setting::kSmartLockUnlockOrSignIn,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSmartLock, kSmartLockSettings,
                            generator);
  generator->RegisterNestedAltSetting(mojom::Setting::kSmartLockOnOff,
                                      mojom::Subpage::kMultiDeviceFeatures);

  // Nearby Share, registered regardless of the flag.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_NEARBY_SHARE_TITLE, mojom::Subpage::kNearbyShare,
      mojom::SearchResultIcon::kNearbyShare,
      mojom::SearchResultDefaultRank::kMedium, mojom::kNearbyShareSubpagePath);
  static constexpr mojom::Setting kNearbyShareSettings[] = {
      mojom::Setting::kNearbyShareOnOff,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kNearbyShare, kNearbyShareSettings,
                            generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kNearbyShareOnOff);
}

void MultiDeviceSection::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetMultiDeviceOptedOutSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInPhoneHubSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInWifiSyncSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInSearchConcepts());

  if (IsOptedIn(host_status_with_device.first)) {
    updater.AddSearchTags(GetMultiDeviceOptedInSearchConcepts());
    if (features::IsPhoneHubEnabled())
      updater.AddSearchTags(GetMultiDeviceOptedInPhoneHubSearchConcepts());
    if (features::IsWifiSyncAndroidEnabled())
      updater.AddSearchTags(GetMultiDeviceOptedInWifiSyncSearchConcepts());
  } else {
    updater.AddSearchTags(GetMultiDeviceOptedOutSearchConcepts());
  }
}

void MultiDeviceSection::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetSmartLockOptionsSearchConcepts());

  if (feature_states_map.at(multidevice_setup::mojom::Feature::kSmartLock) ==
      multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    updater.AddSearchTags(GetSmartLockOptionsSearchConcepts());
  }
}

void MultiDeviceSection::OnNearbySharingEnabledChanged() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (pref_service_->GetBoolean(::prefs::kNearbySharingEnabledPrefName)) {
    updater.RemoveSearchTags(GetNearbyShareOffSearchConcepts());
    updater.AddSearchTags(GetNearbyShareOnSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetNearbyShareOnSearchConcepts());
    updater.AddSearchTags(GetNearbyShareOffSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
