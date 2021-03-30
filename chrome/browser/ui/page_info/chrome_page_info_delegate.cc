// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/chooser_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "ui/events/event.h"
#else
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

ChromePageInfoDelegate::ChromePageInfoDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

Profile* ChromePageInfoDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

permissions::ChooserContextBase* ChromePageInfoDelegate::GetChooserContext(
    ContentSettingsType type) {
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
#if !defined(OS_ANDROID)
      return SerialChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED();
      return nullptr;
#endif
    case ContentSettingsType::HID_CHOOSER_DATA:
#if !defined(OS_ANDROID)
      return HidChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED();
      return nullptr;
#endif
    default:
      NOTREACHED();
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
  std::vector<size_t> placeholder_offsets;
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();

  // |password_protection_service| may be null in test.
  return chrome_password_protection_service
             ? chrome_password_protection_service->GetWarningDetailText(
                   chrome_password_protection_service
                       ->reused_password_account_type_for_last_shown_warning(),
                   &placeholder_offsets)
             : std::u16string();
}
#endif

permissions::PermissionResult ChromePageInfoDelegate::GetPermissionStatus(
    ContentSettingsType type,
    const GURL& site_url) {
  // TODO(raymes): Use GetPermissionStatus() to retrieve information
  // about *all* permissions once it has default behaviour implemented for
  // ContentSettingTypes that aren't permissions.
  return PermissionManagerFactory::GetForProfile(GetProfile())
      ->GetPermissionStatus(type, site_url, site_url);
}
#if !defined(OS_ANDROID)
bool ChromePageInfoDelegate::CreateInfoBarDelegate() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (infobar_service) {
    PageInfoInfoBarDelegate::Create(infobar_service);
    return true;
  }
  return false;
}

void ChromePageInfoDelegate::ShowSiteSettings(const GURL& site_url) {
  chrome::ShowSiteSettings(chrome::FindBrowserWithWebContents(web_contents_),
                           site_url);
}

void ChromePageInfoDelegate::OpenCookiesDialog() {
  TabDialogs::FromWebContents(web_contents_)->ShowCollectedCookies();
}

void ChromePageInfoDelegate::OpenCertificateDialog(
    net::X509Certificate* certificate) {
  gfx::NativeWindow top_window = web_contents_->GetTopLevelNativeWindow();
  DCHECK(certificate);
  DCHECK(top_window);

  ShowCertificateViewer(web_contents_, top_window, certificate);
}

void ChromePageInfoDelegate::OpenConnectionHelpCenterPage(
    const ui::Event& event) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB),
      ui::PAGE_TRANSITION_LINK, false));
}

void ChromePageInfoDelegate::OpenSafetyTipHelpCenterPage() {
  OpenHelpCenterFromSafetyTip(web_contents_);
}
#endif

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

bool ChromePageInfoDelegate::IsContentDisplayedInVrHeadset() {
  return vr::VrTabHelper::IsContentDisplayedInHeadset(web_contents_);
}

security_state::SecurityLevel ChromePageInfoDelegate::GetSecurityLevel() {
  if (security_state_for_tests_set_)
    return security_level_for_tests_;

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  SecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}

security_state::VisibleSecurityState
ChromePageInfoDelegate::GetVisibleSecurityState() {
  if (security_state_for_tests_set_)
    return visible_security_state_for_tests_;

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  SecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return *helper->GetVisibleSecurityState();
}

std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
ChromePageInfoDelegate::GetPageSpecificContentSettingsDelegate() {
  auto delegate = std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
      web_contents_);
  return std::move(delegate);
}

#if defined(OS_ANDROID)
const std::u16string ChromePageInfoDelegate::GetClientApplicationName() {
  return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
}
#endif

void ChromePageInfoDelegate::SetSecurityStateForTests(
    security_state::SecurityLevel security_level,
    security_state::VisibleSecurityState visible_security_state) {
  security_state_for_tests_set_ = true;
  security_level_for_tests_ = security_level;
  visible_security_state_for_tests_ = visible_security_state;
}
