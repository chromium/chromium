// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace settings {

void AddPrivacySandboxStrings(content::WebUIDataSource* html_source,
                              Profile* /*profile*/) {
  // Strings used outside the privacy sandbox page. The i18n preprocessor might
  // replace those before the corresponding flag value is checked, which is why
  // they are included independently of the flag value.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
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
       IDS_SETTINGS_BLOCK_THIRD_PARTY_COOKIES_EXPAND_A11Y_LABEL}};
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
