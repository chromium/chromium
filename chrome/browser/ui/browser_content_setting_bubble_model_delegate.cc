// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

// The URL for when the user clicks "learn more" on the mixed scripting page
// icon bubble.
const char kInsecureScriptHelpUrl[] =
    "https://support.google.com/chrome/?p=unauthenticated";

BrowserContentSettingBubbleModelDelegate::
    BrowserContentSettingBubbleModelDelegate(Browser* browser)
    : browser_(browser) {}

BrowserContentSettingBubbleModelDelegate::
    ~BrowserContentSettingBubbleModelDelegate() {}

void BrowserContentSettingBubbleModelDelegate::ShowCollectedCookiesDialog(
    content::WebContents* web_contents) {
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
}

void BrowserContentSettingBubbleModelDelegate::ShowMediaSettingsPage() {
  // Microphone and camera settings appear in the content settings menu right
  // next to each other, the microphone section is first.
  chrome::ShowContentSettings(browser_, ContentSettingsType::MEDIASTREAM_MIC);
}

void BrowserContentSettingBubbleModelDelegate::ShowContentSettingsPage(
    ContentSettingsType type) {
  if (type == ContentSettingsType::PROTOCOL_HANDLERS)
    chrome::ShowSettingsSubPage(browser_, chrome::kHandlerSettingsSubPage);
  else
    chrome::ShowContentSettingsExceptions(browser_, type);
}

void BrowserContentSettingBubbleModelDelegate::ShowLearnMorePage(
    ContentSettingsType type) {
  GURL learn_more_url;
  switch (type) {
    case ContentSettingsType::PLUGINS:
      learn_more_url = GURL(chrome::kBlockedPluginLearnMoreURL);
      break;
    case ContentSettingsType::ADS:
      learn_more_url = GURL(subresource_filter::kLearnMoreLink);
      break;
    case ContentSettingsType::MIXEDSCRIPT:
      learn_more_url = GURL(kInsecureScriptHelpUrl);
      break;
    default:
      return;
  }
  DCHECK(!learn_more_url.is_empty());
  chrome::AddSelectedTabWithURL(browser_, learn_more_url,
                                ui::PAGE_TRANSITION_LINK);
}
