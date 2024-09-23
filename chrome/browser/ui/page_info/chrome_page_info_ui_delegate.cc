// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
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

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
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
    // TODO(crbug.com/40189322): This string can probably be removed.
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
std::optional<page_info::proto::SiteInfo>
ChromePageInfoUiDelegate::GetAboutThisSiteInfo() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser || !browser->is_type_normal()) {
    // TODO(crbug.com/40904874): SidePanel is not available. Evaluate if we can
    //                          show ATP in a different way.
    return std::nullopt;
  }
  if (auto* service =
          AboutThisSiteServiceFactory::GetForProfile(GetProfile())) {
    return service->GetAboutThisSiteInfo(
        site_url_, web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId(),
        AboutThisSiteTabHelper::FromWebContents(web_contents_));
  }

  return std::nullopt;
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

// TODO(crbug.com/40776829): Reconcile with LastTabStandingTracker.
bool ChromePageInfoUiDelegate::IsMultipleTabsOpen() {
  int count = 0;
  auto site_origin = site_url_.DeprecatedGetOriginAsURL();
  for (extensions::WindowController* window :
       *extensions::WindowControllerList::GetInstance()) {
    for (int i = 0; i < window->GetTabCount(); ++i) {
      content::WebContents* const web_contents = window->GetWebContentsAt(i);
      if (web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL() ==
          site_origin) {
        count++;
      }
    }
  }
  return count > 1;
}

void ChromePageInfoUiDelegate::OpenSiteSettingsFileSystem() {
  chrome::ShowSiteSettingsFileSystem(GetProfile(), site_url_);
}

void ChromePageInfoUiDelegate::ShowPrivacySandboxSettings() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowPrivacySandboxSettings(browser);
}

std::u16string ChromePageInfoUiDelegate::GetPermissionDetail(
    ContentSettingsType type) {
  switch (type) {
    // TODO(crbug.com/40777580): Reconcile with SiteDetailsPermissionElement.
    case ContentSettingsType::ADS:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE);
    default:
      return {};
  }
}

bool ChromePageInfoUiDelegate::ShouldShowSettingsLinkForPermission(
    ContentSettingsType type,
    int* text_id,
    int* link_id) {
  switch (type) {
    case ContentSettingsType::NOTIFICATIONS:
#if BUILDFLAG(IS_MAC)
      // This can be extracted into
      // SystemPermissionSettings::IsPermissionDenied() in a similar way as it
      // is done for camera and mic. I attempted to do this (see
      // https://chromium-review.googlesource.com/c/chromium/src/+/5424111/27..28
      // ), however as we don't have any testcase for this branch, the changes
      // were refused by the test coverage bot.
      // TODO(b/345431801): Add a testcase to cover this case.
      if (base::FeatureList::IsEnabled(
              features::kAppShimNotificationAttribution)) {
        // If this notification permission is associated with a locally
        // installed web app, the corresponding app shim needs to have system
        // level notification permission for notifications to work. If system
        // permissions are missing, guide the user to system settings to fix
        // this.
        std::optional<webapps::AppId> app_id =
            web_app::WebAppTabHelper::GetAppIdForNotificationAttribution(
                web_contents_);
        if (!app_id.has_value()) {
          return false;
        }

        // If the system permission is already granted linking to system
        // settings doesn't really add anything. If system permissions is
        // "not determined", there won't be a page in system settings to link
        // to for this app, and Chrome will still be able to display a system
        // permission prompt, so there is no reason to guide the user to system
        // settings yet.
        auto system_permission_status =
            AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(
                *app_id);
        if (system_permission_status ==
                mac_notifications::mojom::PermissionStatus::kGranted ||
            system_permission_status ==
                mac_notifications::mojom::PermissionStatus::kNotDetermined) {
          return false;
        }
        *text_id = IDS_PAGE_INFO_NOTIFICATIONS_SYSTEM_SETTINGS_DESCRIPTION;
        *link_id = IDS_PAGE_INFO_SYSTEM_SETTINGS_LINK;
        return true;
      }
#endif
      return false;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      if (base::FeatureList::IsEnabled(
              content_settings::features::kLeftHandSideActivityIndicators) &&
          system_permission_settings::IsDenied(type)) {
        *text_id = IDS_PAGE_INFO_CAMERA_SYSTEM_SETTINGS_DESCRIPTION;
        *link_id = IDS_PAGE_INFO_SETTINGS_OF_A_SYSTEM_LINK;
        return true;
      }
      return false;
    case ContentSettingsType::MEDIASTREAM_MIC:
      if (base::FeatureList::IsEnabled(
              content_settings::features::kLeftHandSideActivityIndicators) &&
          system_permission_settings::IsDenied(type)) {
        *text_id = IDS_PAGE_INFO_MICROPHONE_SYSTEM_SETTINGS_DESCRIPTION;
        *link_id = IDS_PAGE_INFO_SETTINGS_OF_A_SYSTEM_LINK;
        return true;
      }
      return false;
#if BUILDFLAG(IS_CHROMEOS)
    case ContentSettingsType::GEOLOCATION:
      if (base::FeatureList::IsEnabled(
              content_settings::features::
                  kCrosSystemLevelPermissionBlockedWarnings) &&
          system_permission_settings::IsDenied(type)) {
        *text_id = IDS_PAGE_INFO_LOCATION_SYSTEM_SETTINGS_DESCRIPTION;
        *link_id = IDS_PAGE_INFO_SETTINGS_OF_A_SYSTEM_LINK;
        return true;
      }
      return false;
#endif
    default:
      return false;
  }
}

void ChromePageInfoUiDelegate::SettingsLinkClicked(ContentSettingsType type) {
  system_permission_settings::OpenSystemSettings(web_contents_, type);
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

bool ChromePageInfoUiDelegate::IsTrackingProtection3pcdEnabled() {
  return TrackingProtectionSettingsFactory::GetForProfile(GetProfile())
      ->IsTrackingProtection3pcdEnabled();
}

std::optional<content::PermissionResult>
ChromePageInfoUiDelegate::GetEmbargoResult(ContentSettingsType type) {
  return permissions::PermissionsClient::Get()
      ->GetPermissionDecisionAutoBlocker(GetProfile())
      ->GetEmbargoResult(site_url_, type);
}

Profile* ChromePageInfoUiDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}
