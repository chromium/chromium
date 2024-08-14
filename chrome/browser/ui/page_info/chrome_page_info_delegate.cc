// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#else
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/events/event.h"
#endif

namespace {

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};

constexpr UrlIdentity::FormatOptions kUrlIdentityOptions{
    .default_options = {UrlIdentity::DefaultFormatOptions::
                            kOmitSchemePathAndTrivialSubdomains}};

}  // namespace

ChromePageInfoDelegate::ChromePageInfoDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  sentiment_service_ =
      TrustSafetySentimentServiceFactory::GetForProfile(GetProfile());
#endif
  base::UmaHistogramBoolean("Security.PageInfo.AboutThisSiteLanguageSupported",
                            page_info::IsAboutThisSiteFeatureEnabled(
                                g_browser_process->GetApplicationLocale()));
}

Profile* ChromePageInfoDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

permissions::ObjectPermissionContextBase*
ChromePageInfoDelegate::GetChooserContext(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      return UsbChooserContextFactory::GetForProfile(GetProfile());
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      if (base::FeatureList::IsEnabled(
              features::kWebBluetoothNewPermissionsBackend)) {
        return BluetoothChooserContextFactory::GetForProfile(GetProfile());
      }
      return nullptr;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
#if !BUILDFLAG(IS_ANDROID)
      return SerialChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED_IN_MIGRATION();
      return nullptr;
#endif
    case ContentSettingsType::HID_CHOOSER_DATA:
#if !BUILDFLAG(IS_ANDROID)
      return HidChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED_IN_MIGRATION();
      return nullptr;
#endif
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::ChromePasswordProtectionService*
ChromePageInfoDelegate::GetChromePasswordProtectionService() const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(GetProfile());
}

safe_browsing::PasswordProtectionService*
ChromePageInfoDelegate::GetPasswordProtectionService() const {
  return GetChromePasswordProtectionService();
}

void ChromePageInfoDelegate::OnUserActionOnPasswordUi(
    safe_browsing::WarningAction action) {
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();
  DCHECK(chrome_password_protection_service);

  chrome_password_protection_service->OnUserAction(
      web_contents_,
      chrome_password_protection_service
          ->reused_password_account_type_for_last_shown_warning(),
      safe_browsing::RequestOutcome::UNKNOWN,
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      /*verdict_token=*/"", safe_browsing::WarningUIType::PAGE_INFO, action);
}

std::u16string ChromePageInfoDelegate::GetWarningDetailText() {
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();

  // |password_protection_service| may be null in test.
  return chrome_password_protection_service
             ? chrome_password_protection_service->GetWarningDetailText(
                   chrome_password_protection_service
                       ->reused_password_account_type_for_last_shown_warning())
             : std::u16string();
}
#endif

content::PermissionResult ChromePageInfoDelegate::GetPermissionResult(
    blink::PermissionType permission,
    const url::Origin& origin,
    const std::optional<url::Origin>& requesting_origin) {
  auto* controller = GetProfile()->GetPermissionController();

  if (requesting_origin.has_value()) {
    return controller->GetPermissionResultForOriginWithoutContext(
        permission, *requesting_origin, origin);
  } else {
    return controller->GetPermissionResultForOriginWithoutContext(permission,
                                                                  origin);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ChromePageInfoDelegate::FocusWebContents() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  browser->ActivateContents(web_contents_);
}

std::optional<std::u16string> ChromePageInfoDelegate::GetRwsOwner(
    const GURL& site_url) {
  return PrivacySandboxServiceFactory::GetForProfile(GetProfile())
      ->GetFirstPartySetOwnerForDisplay(site_url);
}

bool ChromePageInfoDelegate::IsRwsManaged() {
  return PrivacySandboxServiceFactory::GetForProfile(GetProfile())
      ->IsFirstPartySetsDataAccessManaged();
}

bool ChromePageInfoDelegate::CreateInfoBarDelegate() {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents_);
  if (infobar_manager) {
    PageInfoInfoBarDelegate::Create(infobar_manager);
    return true;
  }
  return false;
}

std::unique_ptr<content_settings::CookieControlsController>
ChromePageInfoDelegate::CreateCookieControlsController() {
  Profile* profile = GetProfile();
  return std::make_unique<content_settings::CookieControlsController>(
      CookieSettingsFactory::GetForProfile(profile),
      profile->IsOffTheRecord()
          ? CookieSettingsFactory::GetForProfile(profile->GetOriginalProfile())
          : nullptr,
      HostContentSettingsMapFactory::GetForProfile(profile),
      TrackingProtectionSettingsFactory::GetForProfile(profile));
}

bool ChromePageInfoDelegate::IsIsolatedWebApp() {
  CHECK(web_contents_);
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebContents(web_contents_);
  if (!provider) {
    return false;
  }

  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents_);
  return app_id && provider->registrar_unsafe().IsIsolated(*app_id);
}

void ChromePageInfoDelegate::ShowSiteSettings(const GURL& site_url) {
  if (web_app::HandleAppManagementLinkClickedInPageInfo(web_contents_)) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowSiteSettings(browser, site_url);
}

void ChromePageInfoDelegate::ShowCookiesSettings() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowSettingsSubPage(browser, chrome::kCookieSettingsSubPage);
}

void ChromePageInfoDelegate::ShowAllSitesSettingsFilteredByRwsOwner(
    const std::u16string& rws_owner) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowAllSitesSettingsFilteredByRwsOwner(browser,
                                                 base::UTF16ToUTF8(rws_owner));
}

void ChromePageInfoDelegate::OpenCookiesDialog() {
  FocusWebContents();
  TabDialogs::FromWebContents(web_contents_)->ShowCollectedCookies();
}

void ChromePageInfoDelegate::OpenCertificateDialog(
    net::X509Certificate* certificate) {
  gfx::NativeWindow top_window = web_contents_->GetTopLevelNativeWindow();
  DCHECK(certificate);
  DCHECK(top_window);

  FocusWebContents();
  ShowCertificateViewer(web_contents_, top_window, certificate);
}

void ChromePageInfoDelegate::OpenConnectionHelpCenterPage(
    const ui::Event& event) {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
          ui::DispositionFromEventFlags(
              event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
          ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void ChromePageInfoDelegate::OpenSafetyTipHelpCenterPage() {
  OpenHelpCenterFromSafetyTip(web_contents_);
}

void ChromePageInfoDelegate::OpenContentSettingsExceptions(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD) {
    const GURL& url = web_contents_->GetLastCommittedURL();
    chrome::ShowSiteSettingsFileSystem(GetProfile(), url);
    return;
  }
  chrome::ShowContentSettingsExceptionsForProfile(GetProfile(),
                                                  content_settings_type);
}

void ChromePageInfoDelegate::OnPageInfoActionOccurred(
    page_info::PageInfoAction action) {
  if (sentiment_service_) {
    if (action == page_info::PAGE_INFO_OPENED) {
      sentiment_service_->PageInfoOpened();
    } else {
      sentiment_service_->InteractedWithPageInfo();
    }
  }
}

void ChromePageInfoDelegate::OnUIClosing() {
  if (sentiment_service_) {
    sentiment_service_->PageInfoClosed();
  }
}
#endif

std::u16string ChromePageInfoDelegate::GetSubjectName(const GURL& url) {
  CHECK(web_contents_);
  return UrlIdentity::CreateFromUrl(GetProfile(), url, kUrlIdentityAllowedTypes,
                                    kUrlIdentityOptions)
      .name;
}

permissions::PermissionDecisionAutoBlocker*
ChromePageInfoDelegate::GetPermissionDecisionAutoblocker() {
  return PermissionDecisionAutoBlockerFactory::GetForProfile(GetProfile());
}

StatefulSSLHostStateDelegate*
ChromePageInfoDelegate::GetStatefulSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(GetProfile());
}

HostContentSettingsMap* ChromePageInfoDelegate::GetContentSettings() {
  return HostContentSettingsMapFactory::GetForProfile(GetProfile());
}

bool ChromePageInfoDelegate::IsSubresourceFilterActivated(
    const GURL& site_url) {
  subresource_filter::SubresourceFilterContentSettingsManager*
      settings_manager =
          SubresourceFilterProfileContextFactory::GetForProfile(GetProfile())
              ->settings_manager();

  return settings_manager->GetSiteActivationFromMetadata(site_url);
}

bool ChromePageInfoDelegate::HasAutoPictureInPictureBeenRegistered() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  auto* auto_pip_tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents_);
  return auto_pip_tab_helper &&
         auto_pip_tab_helper->HasAutoPictureInPictureBeenRegistered();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromePageInfoDelegate::IsContentDisplayedInVrHeadset() {
  return vr::VrTabHelper::IsContentDisplayedInHeadset(web_contents_);
}

security_state::SecurityLevel ChromePageInfoDelegate::GetSecurityLevel() {
  if (security_state_for_tests_set_) {
    return security_level_for_tests_;
  }

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}

security_state::VisibleSecurityState
ChromePageInfoDelegate::GetVisibleSecurityState() {
  if (security_state_for_tests_set_) {
    return visible_security_state_for_tests_;
  }

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return *helper->GetVisibleSecurityState();
}

void ChromePageInfoDelegate::OnCookiesPageOpened() {
  auto* profile = GetProfile();
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  // Don't record the preference if 3PC are allowed by default. Since then
  // cookie controls are not available in the cookies page.
  if (!cookie_settings || !cookie_settings->ShouldBlockThirdPartyCookies()) {
    return;
  }

  profile->GetPrefs()->SetBoolean(prefs::kInContextCookieControlsOpened, true);
}

std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
ChromePageInfoDelegate::GetPageSpecificContentSettingsDelegate() {
  auto delegate =
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents_);
  return std::move(delegate);
}

#if BUILDFLAG(IS_ANDROID)
const std::u16string ChromePageInfoDelegate::GetClientApplicationName() {
  return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
}
#endif

bool ChromePageInfoDelegate::IsHttpsFirstModeEnabled() {
  bool https_first_mode_fully_enabled =
      GetProfile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  bool https_first_mode_enabled_in_incognito =
      base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      GetProfile()->GetPrefs()->GetBoolean(prefs::kHttpsFirstModeIncognito);
  return https_first_mode_fully_enabled ||
         (GetProfile()->IsIncognitoProfile() &&
          https_first_mode_enabled_in_incognito);
}

void ChromePageInfoDelegate::SetSecurityStateForTests(
    security_state::SecurityLevel security_level,
    security_state::VisibleSecurityState visible_security_state) {
  security_state_for_tests_set_ = true;
  security_level_for_tests_ = security_level;
  visible_security_state_for_tests_ = visible_security_state;
}
