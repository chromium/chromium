// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

void AddPrivacySandboxStrings(content::WebUIDataSource* html_source,
                              Profile* profile) {
  // Strings used outside the privacy sandbox page. The i18n preprocessor might
  // replace those before the corresponding flag value is checked, which is why
  // they are included independently of the flag value.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacySandboxCookiesDialog",
       IDS_SETTINGS_PRIVACY_SANDBOX_COOKIES_DIALOG},
      {"privacySandboxCookiesDialogMore",
       IDS_SETTINGS_PRIVACY_SANDBOX_COOKIES_DIALOG_MORE},
      {"privacySandboxLearnMoreDialogTopicsDataTypes",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_TYPES},
      {"privacySandboxLearnMoreDialogTopicsDataUsage",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_USAGE},
      {"privacySandboxLearnMoreDialogTopicsDataManagement",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_MANAGEMENT},
      {"privacySandboxLearnMoreDialogFledgeDataTypes",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_DATA_TYPES},
      {"privacySandboxLearnMoreDialogFledgeDataUsage",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_DATA_USAGE},
      {"privacySandboxAdPersonalizationDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION},
      {"privacySandboxAdPersonalizationDialogDescriptionTrialsOff",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION_TRIALS_OFF},
      {"privacySandboxAdPersonalizationDialogDescriptionListsEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION_LISTS_EMPTY},
      {"privacySandboxAdPersonalizationDialogTopicsTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_TITLE},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore1",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_1},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore2",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_2},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore3",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_3},
      {"privacySandboxAdPersonalizationDialogFledgeLearnMore1",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_LEARN_MORE_1},
      {"privacySandboxAdMeasurementDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_DESCRIPTION},
      {"privacySandboxAdMeasurementDialogDescriptionTrialsOff",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_DESCRIPTION_TRIALS_OFF},
      {"adPrivacyLinkRowLabel", IDS_SETTINGS_AD_PRIVACY_LINK_ROW_LABEL},
      {"adPrivacyLinkRowSubLabel", IDS_SETTINGS_AD_PRIVACY_LINK_ROW_SUB_LABEL},
      {"adPrivacyRestrictedLinkRowSubLabel",
       IDS_SETTINGS_AD_PRIVACY_RESTRICTED_LINK_ROW_SUB_LABEL},
      {"adPrivacyPageTitle", IDS_SETTINGS_AD_PRIVACY_PAGE_TITLE},
      {"adPrivacyPageTopicsLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_LABEL},
      {"adPrivacyPageTopicsLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageTopicsLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_SUB_LABEL_DISABLED},
      {"adPrivacyPageFledgeLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_LABEL},
      {"adPrivacyPageFledgeLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageFledgeLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_SUB_LABEL_DISABLED},
      {"adPrivacyPageAdMeasurementLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_LABEL},
      {"adPrivacyPageAdMeasurementLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageAdMeasurementLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_SUB_LABEL_DISABLED},
      {"topicsPageTitle", IDS_SETTINGS_TOPICS_PAGE_TITLE},
      {"topicsPageToggleLabel", IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL},
      {"topicsPageToggleSubLabel", IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL},
      {"topicsPageToggleSubLabelV2",
       IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2},
      {"topicsPageCurrentTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING},
      {"topicsPageActiveTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING},
      {"topicsPageCurrentTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION},
      {"topicsPageActiveTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_DESCRIPTION},
      {"topicsPageCurrentTopicsDescriptionLearnMoreLink",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_LEARN_MORE_LINK},
      {"topicsPageCurrentTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageLearnMoreHeading",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING},
      {"topicsPageLearnMoreBullet1",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1},
      {"topicsPageLearnMoreBullet2",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2},
      {"topicsPageCurrentTopicsDescriptionDisabled",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED},
      {"topicsPageCurrentTopicsDescriptionEmpty",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY},
      {"topicsPageCurrentTopicsDescriptionEmptyTextHeading",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_HEADING},
      {"topicsPageCurrentTopicsDescriptionEmptyTextV2",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2},
      {"topicsPageBlockedTopicsDescriptionEmptyTextHeading",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_HEADING},
      {"topicsPageBlockedTopicsDescriptionEmptyTextV2",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2},
      {"topicsPageBlockTopic", IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC},
      {"topicsPageBlockTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC_A11Y_LABEL},
      {"topicsPageBlockedTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING},
      {"topicsPageBlockedTopicsHeadingNew",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW},
      {"topicsPageBlockedTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION},
      {"topicsPageBlockedTopicsDescriptionEmpty",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY},
      {"topicsPageBlockedTopicsDescriptionNew",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW},
      {"topicsPageBlockedTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageAllowTopic", IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC},
      {"topicsPageAllowTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC_A11Y_LABEL},
      {"topicsPageUnblockTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_UNBLOCK_TOPIC_A11Y_LABEL},
      {"topicsPageCurrentTopicsDescriptionLearnMoreA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_LEARN_MORE_A11Y_LABEL},
      {"fledgePageTitle", IDS_SETTINGS_FLEDGE_PAGE_TITLE},
      {"fledgePageToggleLabel", IDS_SETTINGS_FLEDGE_PAGE_TOGGLE_LABEL},
      {"fledgePageToggleSubLabel", IDS_SETTINGS_FLEDGE_PAGE_TOGGLE_SUB_LABEL},
      {"fledgePageCurrentSitesHeading",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_HEADING},
      {"fledgePageCurrentSitesDescription",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION},
      {"fledgePageCurrentSitesDescriptionLearnMore",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_LEARN_MORE},
      {"fledgePageCurrentSitesDescriptionDisabled",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_DISABLED},
      {"fledgePageCurrentSitesDescriptionEmpty",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_EMPTY},
      {"fledgePageCurrentSitesRegionA11yDescription",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_REGION_A11Y_DESCRIPTION},
      {"fledgePageSeeAllSitesLabel",
       IDS_SETTINGS_FLEDGE_PAGE_SEE_ALL_SITES_LABEL},
      {"fledgePageBlockSite", IDS_SETTINGS_FLEDGE_PAGE_BLOCK_SITE},
      {"fledgePageBlockSiteA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCK_SITE_A11Y_LABEL},
      {"fledgePageBlockedSitesHeading",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_HEADING},
      {"fledgePageBlockedSitesDescription",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_DESCRIPTION},
      {"fledgePageBlockedSitesDescriptionEmpty",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_DESCRIPTION_EMPTY},
      {"fledgePageBlockedSitesRegionA11yDescription",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_REGION_A11Y_DESCRIPTION},
      {"fledgePageAllowSite", IDS_SETTINGS_FLEDGE_PAGE_ALLOW_SITE},
      {"fledgePageAllowSiteA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_ALLOW_SITE_A11Y_LABEL},
      {"fledgePageLearnMoreHeading",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_HEADING},
      {"fledgePageLearnMoreBullet1",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_1},
      {"fledgePageLearnMoreBullet2",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_2},
      {"fledgePageCurrentSitesDescriptionLearnMoreA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_LEARN_MORE_A11Y_LABEL},
      {"adMeasurementPageTitle", IDS_SETTINGS_AD_MEASUREMENT_PAGE_TITLE},
      {"adMeasurementPageToggleLabel",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_TOGGLE_LABEL},
      {"adMeasurementPageToggleSubLabel",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_TOGGLE_SUB_LABEL},
      {"adMeasurementPageEnabledBullet1",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_1},
      {"adMeasurementPageEnabledBullet2",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_2},
      {"adMeasurementPageEnabledBullet3",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_3},
      {"adMeasurementPageConsiderBullet1",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_BULLET_1},
      {"adMeasurementPageConsiderBullet2",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_BULLET_2},
      {"manageTopicsPageDescription",
       IDS_SETTINGS_MANAGE_TOPICS_PAGE_DESCRIPTION},
      {"manageTopicsPageLearnMoreLink",
       IDS_SETTINGS_MANAGE_TOPICS_PAGE_LEARN_MORE_LINK},
      {"manageTopicsHeading", IDS_SETTINGS_TOPICS_PAGE_MANAGE_TOPICS_HEADING},
      {"manageTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_MANAGE_TOPICS_DESCRIPTION},
      {"manageTopicsDialogTitle", IDS_SETTINGS_MANAGE_TOPICS_DIALOG_TITLE},
      {"manageTopicsDialogBody", IDS_SETTINGS_MANAGE_TOPICS_DIALOG_BODY},
      {"unblockTopicToastBody", IDS_SETTINGS_UNBLOCK_TOPIC_TOAST_BODY},
      {"unblockTopicToastButtonText",
       IDS_SETTINGS_UNBLOCK_TOPIC_TOAST_BUTTON_TEXT},
      {"fledgePageExplanation", IDS_SETTINGS_FLEDGE_PAGE_EXPLANATION},
      {"unblockTopicButtonTextV2", IDS_SETTINGS_UNBLOCK_TOPIC_BUTTON_TEXT_V2},
      {"privacyGuideAdTopicsHeading",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_HEADING},
      {"privacyGuideAdTopicsToggleLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_TOGGLE_LABEL},
      {"privacyGuideAdTopicsWhenOnBullet1",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_WHEN_ON_BULLET1},
      {"privacyGuideAdTopicsWhenOnBullet2",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_WHEN_ON_BULLET2},
      {"privacyGuideAdTopicsThingsToConsiderBullet1",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_THINGS_TO_CONSIDER_BULLET1},
      {"privacyGuideAdTopicsThingsToConsiderBullet2",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_THINGS_TO_CONSIDER_BULLET2},
      {"tpcAndKnownTrackersPageTitle",
       IDS_SETTINGS_3PC_AND_KNOWN_TRACKERS_PAGE_TITLE},
      {"tpcAndKnownTrackersExceptionsListTitle",
       IDS_SETTINGS_3PC_AND_KNOWN_TRACKERS_EXCEPTIONS_LIST_TITLE},
      {"tpcAndKnownTrackersExceptionsListDescription",
       IDS_SETTINGS_3PC_AND_KNOWN_TRACKERS_EXCEPTIONS_LIST_DESCRIPTION},
      {"tpcAndKnownTrackersLinkRowLabel",
       IDS_SETTINGS_3PC_AND_KNOWN_TRACKERS_LINK_ROW_LABEL},
      {"trackingProtectionDefaultHeader",
       IDS_SETTINGS_TRACKING_PROTECTION_DEFAULT_HEADER},
      {"trackingProtectionTpcdBulletOne",
       IDS_SETTINGS_TRACKING_PROTECTION_TPCD_BULLET_ONE},
      {"trackingProtectionTpcdBulletTwoDescription",
       IDS_SETTINGS_TRACKING_PROTECTION_TPCD_BULLET_TWO_DESCRIPTION},
      {"trackingProtectionAdditionalProtectionsHeader",
       IDS_SETTINGS_TRACKING_PROTECTION_ADDITIONAL_PROTECTIONS_HEADER},
      {"trackingProtectionBlockAll3pcsSubLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_BLOCK_ALL_3PCS_SUB_LABEL},
      {"trackingProtectionExceptionsListTitle",
       IDS_SETTINGS_TRACKING_PROTECTION_EXCEPTIONS_LIST_TITLE},
      {"trackingProtectionExceptionsListDescription",
       IDS_SETTINGS_TRACKING_PROTECTION_EXCEPTIONS_LIST_DESCRIPTION},

  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("adPrivacyLearnMoreURL",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kAdPrivacyLearnMoreURL),
                             g_browser_process->GetApplicationLocale())
                             .spec());

  // Tracking Protection strings containing HC links
  html_source->AddString(
      "trackingProtectionDefaultDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TRACKING_PROTECTION_DEFAULT_DESCRIPTION,
          // TODO(https://b/350525567): Update with finalized URL
          chrome::kUserBypassHelpCenterURL,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_TRACKING_PROTECTION_DEFAULT_LEARN_MORE_ARIA_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  html_source->AddString(
      "trackingProtectionAdditionalProtectionsDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TRACKING_PROTECTION_ADDITIONAL_PROTECTIONS_DESCRIPTION,
          // TODO(https://b/350525567): Update with finalized URL
          chrome::kUserBypassHelpCenterURL,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_TRACKING_PROTECTION_ADDITIONAL_PROTECTIONS_LEARN_MORE_ARIA_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));

  // Topics and fledge link to help center articles in their learn more dialog.
  html_source->AddString(
      "topicsPageLearnMoreBullet3",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3,
          base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                                 GURL(chrome::kAdPrivacyLearnMoreURL),
                                 g_browser_process->GetApplicationLocale())
                                 .spec())));
  html_source->AddString(
      "fledgePageLearnMoreBullet3",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_3,
          base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                                 GURL(chrome::kAdPrivacyLearnMoreURL),
                                 g_browser_process->GetApplicationLocale())
                                 .spec())));
  html_source->AddString(
      "manageTopicsPageLearnMoreLink",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MANAGE_TOPICS_PAGE_LEARN_MORE_LINK,
          chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_MANAGE_TOPICS_PAGE_DESCRIPTION_LEARN_MORE_ARIA_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  html_source->AddString(
      "topicsPageDisclaimerDesktop",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_DISCLAIMER_DESKTOP,
          chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  // Topics and fledge both link to the cookies setting page and cross-link
  // each other in the footers.
  html_source->AddString(
      "topicsPageFooter",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_TOPICS_PAGE_FOOTER,
                                 chrome::kChromeUIPrivacySandboxFledgeURL,
                                 chrome::kChromeUICookieSettingsURL));
  html_source->AddString(
      "topicsPageFooterV2",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_FOOTER_V2,
          chrome::kChromeUIPrivacySandboxFledgeURL,
          chrome::kChromeUICookieSettingsURL,
          chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL));
  html_source->AddString(
      "fledgePageFooterV2",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_FLEDGE_PAGE_FOOTER_V2,
          chrome::kChromeUIPrivacySandboxTopicsURL,
          chrome::kChromeUICookieSettingsURL,
          chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL));
  html_source->AddBoolean(
      "firstPartySetsUIEnabled",
      base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI));
}

}  // namespace settings
