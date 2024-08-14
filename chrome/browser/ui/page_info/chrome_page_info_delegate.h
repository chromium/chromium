// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/page_info/page_info_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class Profile;
class StatefulSSLHostStateDelegate;
class TrustSafetySentimentService;

namespace content_settings {
class PageSpecificContentSettings;
}

namespace permissions {
class ObjectPermissionContextBase;
class PermissionDecisionAutoBlocker;
}  // namespace permissions

namespace safe_browsing {
class PasswordProtectionService;
class ChromePasswordProtectionService;
}  // namespace safe_browsing

class ChromePageInfoDelegate : public PageInfoDelegate {
 public:
  explicit ChromePageInfoDelegate(content::WebContents* web_contents);
  ~ChromePageInfoDelegate() override = default;

  void SetSecurityStateForTests(
      security_state::SecurityLevel security_level,
      security_state::VisibleSecurityState visible_security_state);

  // PageInfoDelegate implementation
  permissions::ObjectPermissionContextBase* GetChooserContext(
      ContentSettingsType type) override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
  void OnUserActionOnPasswordUi(safe_browsing::WarningAction action) override;
  std::u16string GetWarningDetailText() override;
#endif
  content::PermissionResult GetPermissionResult(
      blink::PermissionType permission,
      const url::Origin& origin,
      const std::optional<url::Origin>& requesting_origin) override;
#if !BUILDFLAG(IS_ANDROID)
  std::optional<std::u16string> GetRwsOwner(const GURL& site_url) override;
  bool IsRwsManaged() override;
  bool CreateInfoBarDelegate() override;
  std::unique_ptr<content_settings::CookieControlsController>
  CreateCookieControlsController() override;
  bool IsIsolatedWebApp() override;
  // In Chrome's case, this may show the site settings page or an app settings
  // page, depending on context.
  void ShowSiteSettings(const GURL& site_url) override;
  void ShowCookiesSettings() override;
  void ShowAllSitesSettingsFilteredByRwsOwner(
      const std::u16string& rws_owner) override;
  void OpenCookiesDialog() override;
  void OpenCertificateDialog(net::X509Certificate* certificate) override;
  void OpenConnectionHelpCenterPage(const ui::Event& event) override;
  void OpenSafetyTipHelpCenterPage() override;
  void OpenContentSettingsExceptions(
      ContentSettingsType content_settings_type) override;
  void OnPageInfoActionOccurred(page_info::PageInfoAction action) override;
  void OnUIClosing() override;
#endif

  std::u16string GetSubjectName(const GURL& url) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoblocker()
      override;
  StatefulSSLHostStateDelegate* GetStatefulSSLHostStateDelegate() override;
  HostContentSettingsMap* GetContentSettings() override;
  bool IsSubresourceFilterActivated(const GURL& site_url) override;
  bool HasAutoPictureInPictureBeenRegistered() override;
  bool IsContentDisplayedInVrHeadset() override;
  security_state::SecurityLevel GetSecurityLevel() override;
  security_state::VisibleSecurityState GetVisibleSecurityState() override;
  void OnCookiesPageOpened() override;
  std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
  GetPageSpecificContentSettingsDelegate() override;

#if BUILDFLAG(IS_ANDROID)
  const std::u16string GetClientApplicationName() override;
#endif

  bool IsHttpsFirstModeEnabled() override;

 private:
  Profile* GetProfile() const;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::ChromePasswordProtectionService*
  GetChromePasswordProtectionService() const;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Focus the window and tab for the web contents.
  void FocusWebContents();

  // The sentiment service is owned by the profile and will outlive this. The
  // service cannot be retrieved via |web_contents_| as that may be destroyed
  // before this is.
  raw_ptr<TrustSafetySentimentService> sentiment_service_;
#endif

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
  security_state::SecurityLevel security_level_for_tests_;
  security_state::VisibleSecurityState visible_security_state_for_tests_;
  bool security_state_for_tests_set_ = false;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
