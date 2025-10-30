// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
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
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace settings {

PrivacySandboxService* GetPrivacySandboxService(Profile* profile) {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_service);
  return privacy_sandbox_service;
}

// The name of the on-click function when the privacy policy link is pressed.
constexpr char16_t kPrivacyPolicyFunc[] = u"onPrivacyPolicyLinkClicked_";

// The id of the html element that opens the privacy policy link.
inline constexpr char16_t kPrivacyPolicyId[] = u"privacyPolicyLink";

// The V2 id of the html element that opens the privacy policy link.
inline constexpr char16_t kPrivacyPolicyIdV2[] = u"privacyPolicyLinkV2";

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
      {"privacySandboxAdMeasurementDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_DESCRIPTION},
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
      {"topicsPageToggleSubLabel",
       IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2},
      {"topicsPageActiveTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING},
      {"topicsPageActiveTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_DESCRIPTION},
      {"topicsPageCurrentTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageCurrentTopicsDescriptionDisabled",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED},
      {"topicsPageCurrentTopicsDescriptionEmptyTextHeading",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_HEADING},
      {"topicsPageCurrentTopicsDescriptionEmptyText",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2},
      {"topicsPageBlockedTopicsDescriptionEmptyTextHeading",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_HEADING},
      {"topicsPageBlockedTopicsDescriptionEmptyText",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2},
      {"topicsPageBlockTopic", IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC},
      {"topicsPageBlockTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC_A11Y_LABEL},
      {"topicsPageBlockedTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW},
      {"topicsPageBlockedTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW},
      {"topicsPageBlockedTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageAllowTopic", IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC},
      {"topicsPageAllowTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC_A11Y_LABEL},
      {"topicsPageUnblockTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_UNBLOCK_TOPIC_A11Y_LABEL},
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
      // Site Suggested Ads Page - Ads API UX Enhancements
      {"siteSuggestedAdsPageToggleSubLabelV2",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_TOGGLE_SUB_LABEL_V2},
      {"siteSuggestedAdsPageExplanationV2",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_EXPLANATION_V2},
      {"siteSuggestedAdsPageExplanationV2LinkText",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_EXPLANATION_V2_LINK_TEXT},
      {"siteSuggestedAdsPageExplanationV2LinkAriaDescription",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_EXPLANATION_V2_LINK_ARIA_DESCRIPTION},
      {"siteSuggestedAdsPageLearnMoreBullet1V2",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_LEARN_MORE_BULLET_1_V2},
      {"siteSuggestedAdsPageLearnMoreBullet2V2",
       IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_LEARN_MORE_BULLET_2_V2},
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
      {"unblockTopicButtonTextV2", IDS_SETTINGS_UNBLOCK_TOPIC_BUTTON_TEXT_V2},
      {"privacyGuideAdTopicsHeading",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_HEADING},
      {"privacyGuideAdTopicsToggleLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_TOGGLE_LABEL},
      {"privacyGuideAdTopicsWhenOnBullet1",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_WHEN_ON_BULLET1},
      {"privacyGuideAdTopicsWhenOnBullet2",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_WHEN_ON_BULLET2},
      {"privacyGuideAdTopicsWhenOnBullet3",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_WHEN_ON_BULLET3},
      {"privacyGuideAdTopicsThingsToConsiderBullet1",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_THINGS_TO_CONSIDER_BULLET1},
      {"privacyGuideAdTopicsThingsToConsiderBullet2",
       IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_THINGS_TO_CONSIDER_BULLET2},
      {"cookiePageSettingsAllowBulletOne",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_ONE},
      {"cookiePageSettingsAllowBulletTwo",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_TWO},
      {"cookiePageSettingsAllowBulletThree",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_THREE},
      {"cookiePageSettingsBlockBulletOne",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_ONE},
      {"cookiePageSettingsBlockBulletTwo",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_TWO},
      {"cookiePageSettingsBlockBulletThree",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_THREE},
      {"privacyGuideCookieSettingsAllowWhenOnBulletOne",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_ALLOW_WHEN_ON_BULLET_ONE},
      {"privacyGuideCookieSettingsAllowWhenOnBulletTwo",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_ALLOW_WHEN_ON_BULLET_TWO},
      {"privacyGuideCookieSettingsAllowThingsToConsiderBulletOne",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_ALLOW_THINGS_TO_CONSIDER_BULLET_ONE},
      {"privacyGuideCookieSettingsAllowThingsToConsiderBulletTwo",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_ALLOW_THINGS_TO_CONSIDER_BULLET_TWO},
      {"privacyGuideCookieSettingsBlockWhenOnBulletOne",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_BLOCK_WHEN_ON_BULLET_ONE},
      {"privacyGuideCookieSettingsBlockWhenOnBulletTwo",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_BLOCK_WHEN_ON_BULLET_TWO},
      {"privacyGuideCookieSettingsBlockThingsToConsiderBulletOne",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_BLOCK_THINGS_TO_CONSIDER_BULLET_ONE},
      {"privacyGuideCookieSettingsBlockThingsToConsiderBulletTwo",
       IDS_PRIVACY_GUIDE_COOKIE_SETTINGS_BLOCK_THINGS_TO_CONSIDER_BULLET_TWO},
      {"privacyGuideCookiesCardBlockTpcAllowSubheader",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_ALLOW_SUBHEADER},
      {"privacyGuideCookiesCardBlockTpcBlockSubheader",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_BLOCK_SUBHEADER},
      {"allowThirdPartyCookiesExpandA11yLabel",
       IDS_SETTINGS_ALLOW_THIRD_PARTY_COOKIES_EXPAND_A11Y_LABEL},
      {"blockThirdPartyCookiesExpandA11yLabel",
       IDS_SETTINGS_BLOCK_THIRD_PARTY_COOKIES_EXPAND_A11Y_LABEL},
      // Ad Topics Content Parity - Ad Topics Settings
      {"adTopicsPageToggleSubLabel",
       IDS_SETTINGS_AD_TOPICS_PAGE_TOGGLE_SUB_LABEL},
      {"adTopicsPageActiveTopicsDescription",
       IDS_SETTINGS_AD_TOPICS_PAGE_ACTIVE_TOPICS_DESCRIPTION}};
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("adPrivacyLearnMoreURL",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kAdPrivacyLearnMoreURL),
                             g_browser_process->GetApplicationLocale())
                             .spec());

  // Fledge link to help center articles in their learn more dialog.
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
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_FOOTER_V2,
          {chrome::kChromeUIPrivacySandboxFledgeURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
           chrome::kChromeUICookieSettingsURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
           chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)},
          nullptr));
  html_source->AddString(
      "fledgePageFooter",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_FLEDGE_PAGE_FOOTER_V2,
          {chrome::kChromeUIPrivacySandboxTopicsURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
           chrome::kChromeUICookieSettingsURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
           chrome::kChromeUIPrivacySandboxManageTopicsLearnMoreURL,
           l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)},
          nullptr));
  html_source->AddBoolean(
      "isPrivacySandboxAdsApiUxEnhancementsEnabled",
      base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements));
  // Site Suggested Ads Page - Ads API UX Enhancements
  html_source->AddString(
      "siteSuggestedAdsPageLearnMoreBullet3V2",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_LEARN_MORE_BULLET_3_V2,
          base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                                 GURL(chrome::kAdPrivacyLearnMoreURL),
                                 g_browser_process->GetApplicationLocale())
                                 .spec()),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_LEARN_MORE_BULLET_3_V2_LINK_ARIA_DESCRIPTION)));

  bool should_use_china_domain =
      GetPrivacySandboxService(profile)->ShouldUsePrivacyPolicyChinaDomain();
  const char* privacy_policy_url = should_use_china_domain
                                       ? chrome::kPrivacyPolicyURLChina
                                       : chrome::kPrivacyPolicyURL;

  html_source->AddString(
      "siteSuggestedAdsPageDisclaimer",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER,
          base::ASCIIToUTF16(privacy_policy_url),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER_LINK_ARIA_DESCRIPTION),
          kPrivacyPolicyFunc, kPrivacyPolicyId));
  html_source->AddString(
      "siteSuggestedAdsFooterV2Desktop",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_FOOTER_V2_DESKTOP,
          chrome::kChromeUIPrivacySandboxTopicsURL,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
          chrome::kChromeUICookieSettingsURL,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  // Ad Topics Page - Ads API UX Enhancements
  html_source->AddString(
      "adTopicsPageDisclaimer",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_AD_TOPICS_PAGE_DISCLAIMER,
          base::ASCIIToUTF16(privacy_policy_url),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER_LINK_ARIA_DESCRIPTION),
          kPrivacyPolicyFunc, kPrivacyPolicyId));
  html_source->AddString(
      "adTopicsPageFooterV2Desktop",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_AD_TOPICS_PAGE_FOOTER_V2_DESKTOP,
          chrome::kChromeUIPrivacySandboxFledgeURL,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB),
          chrome::kChromeUICookieSettingsURL,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  // Ad Measurement Page - Ads API UX Enhancements
  html_source->AddString(
      "adMeasurementPageDisclaimer",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_AD_MEASUREMENT_PAGE_DISCLAIMER,
          base::ASCIIToUTF16(privacy_policy_url),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER_LINK_ARIA_DESCRIPTION),
          kPrivacyPolicyFunc, kPrivacyPolicyId));
  // Ad Topics Content Parity - Ad Topics Settings
  html_source->AddString(
      "adTopicsPageDisclaimerV2Desktop",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_AD_TOPICS_PAGE_DISCLAIMER_V2_DESKTOP,
          base::ASCIIToUTF16(privacy_policy_url),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER_LINK_ARIA_DESCRIPTION),
          kPrivacyPolicyFunc, kPrivacyPolicyIdV2));
  // Privacy Guide Ad Topics Card - Ad Topics Content Parity
  html_source->AddString(
      "privacyGuideAdTopicsThingsToConsiderBullet3Desktop",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PRIVACY_GUIDE_AD_TOPICS_THINGS_TO_CONSIDER_BULLET3_DESKTOP,
          base::ASCIIToUTF16(privacy_policy_url),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SUGGESTED_ADS_PAGE_DISCLAIMER_LINK_ARIA_DESCRIPTION),
          kPrivacyPolicyFunc, kPrivacyPolicyId));
}

}  // namespace settings
