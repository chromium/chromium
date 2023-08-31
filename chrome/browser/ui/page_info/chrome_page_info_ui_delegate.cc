// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#endif

ChromePageInfoUiDelegate::ChromePageInfoUiDelegate(
    content::WebContents* web_contents,
    const GURL& site_url)
    : web_contents_(web_contents), site_url_(site_url) {}

bool ChromePageInfoUiDelegate::ShouldShowAllow(ContentSettingsType type) {
  switch (type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION:
      return !GetProfile()->IsOffTheRecord();
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return network::IsUrlPotentiallyTrustworthy(site_url_);
    // Chooser permissions do not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::HID_GUARD:
    // Bluetooth scanning does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::BLUETOOTH_SCANNING:
    // File system write does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return false;
    default:
      return true;
  }
}

std::u16string ChromePageInfoUiDelegate::GetAutomaticallyBlockedReason(
    ContentSettingsType type) {
  switch (type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION: {
      if (GetProfile()->IsOffTheRecord()) {
        return l10n_util::GetStringUTF16(
            GetProfile()->IsGuestSession()
                ? IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_IN_GUEST
                : IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_IN_INCOGNITO);
      }
      break;
    }
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    // TODO(crbug.com/1227679): This string can probably be removed.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA: {
      if (!network::IsUrlPotentiallyTrustworthy(site_url_)) {
        return l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_INSECURE);
      }
      break;
    }
    default:
      break;
  }

  return std::u16string();
}

#if !BUILDFLAG(IS_ANDROID)
absl::optional<page_info::proto::SiteInfo>
ChromePageInfoUiDelegate::GetAboutThisSiteInfo() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser || !browser->is_type_normal()) {
    // TODO(crbug.com/1435450): SidePanel is not available. Evaluate if we can
    //                          show ATP in a different way.
    return absl::nullopt;
  }
  if (auto* service =
          AboutThisSiteServiceFactory::GetForProfile(GetProfile())) {
    return service->GetAboutThisSiteInfo(
        site_url_, web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId(),
        AboutThisSiteTabHelper::FromWebContents(web_contents_));
  }

  return absl::nullopt;
}

void ChromePageInfoUiDelegate::OpenMoreAboutThisPageUrl(
    const GURL& url,
    const ui::Event& event) {
  DCHECK(page_info::IsAboutThisSiteFeatureEnabled());
  ShowAboutThisSiteSidePanel(web_contents_, url);
}
#endif

bool ChromePageInfoUiDelegate::ShouldShowAsk(ContentSettingsType type) {
  return permissions::PermissionUtil::IsGuardContentSetting(type);
}

#if !BUILDFLAG(IS_ANDROID)
bool ChromePageInfoUiDelegate::ShouldShowSiteSettings(int* link_text_id,
                                                      int* tooltip_text_id) {
  if (GetProfile()->IsGuestSession())
    return false;

  if (web_app::GetLabelIdsForAppManagementLinkInPageInfo(
          web_contents_, link_text_id, tooltip_text_id)) {
    return true;
  }

  *link_text_id = IDS_PAGE_INFO_SITE_SETTINGS_LINK;
  *tooltip_text_id = IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP;

  return true;
}

// TODO(crbug.com/1227074): Reconcile with LastTabStandingTracker.
bool ChromePageInfoUiDelegate::IsMultipleTabsOpen() {
  const extensions::WindowControllerList::ControllerList& windows =
      extensions::WindowControllerList::GetInstance()->windows();
  int count = 0;
  auto site_origin = site_url_.DeprecatedGetOriginAsURL();
  for (auto* window : windows) {
    const Browser* const browser = window->GetBrowser();
    if (!browser)
      continue;
    const TabStripModel* const tabs = browser->tab_strip_model();
    DCHECK(tabs);
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* const web_contents = tabs->GetWebContentsAt(i);
      if (web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL() ==
          site_origin) {
        count++;
      }
    }
  }
  return count > 1;
}

void ChromePageInfoUiDelegate::ShowPrivacySandboxAdPersonalization() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowPrivacySandboxAdPersonalization(browser);
}

void ChromePageInfoUiDelegate::ShowPrivacySandboxSettings() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowPrivacySandboxSettings(browser);
}

std::u16string ChromePageInfoUiDelegate::GetPermissionDetail(
    ContentSettingsType type) {
  switch (type) {
    // TODO(crbug.com/1228243): Reconcile with SiteDetailsPermissionElement.
    case ContentSettingsType::ADS:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE);
    default:
      return {};
  }
}

bool ChromePageInfoUiDelegate::IsBlockAutoPlayEnabled() {
  return GetProfile()->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
}
#endif

content::PermissionResult ChromePageInfoUiDelegate::GetPermissionResult(
    blink::PermissionType permission) {
  return GetProfile()
      ->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(
          permission, url::Origin::Create(site_url_));
}

absl::optional<content::PermissionResult>
ChromePageInfoUiDelegate::GetEmbargoResult(ContentSettingsType type) {
  return permissions::PermissionsClient::Get()
      ->GetPermissionDecisionAutoBlocker(GetProfile())
      ->GetEmbargoResult(site_url_, type);
}

Profile* ChromePageInfoUiDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}
