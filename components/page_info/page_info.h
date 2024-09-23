// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_PAGE_INFO_H_
#define COMPONENTS_PAGE_INFO_PAGE_INFO_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/page_info/core/page_info_action.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "net/base/schemeful_site.h"

namespace net {
class X509Certificate;
}

namespace permissions {
class ObjectPermissionContextBase;
}

namespace ui {
class Event;
}

class GURL;
class HostContentSettingsMap;
class PageInfoDelegate;
class PageInfoUI;

// The |PageInfo| provides information about a website's permissions,
// connection state and its identity. It owns a UI that displays the
// information and allows users to change the permissions. |PageInfo|
// objects must be created on the heap. They destroy themselves after the UI is
// closed.
class PageInfo : private content_settings::CookieControlsObserver,
                 public content_settings::PageSpecificContentSettings::
                     PermissionUsageObserver {
 public:
  // Status of a connection to a website.
  enum SiteConnectionStatus {
    SITE_CONNECTION_STATUS_UNKNOWN = 0,  // No status available.
    SITE_CONNECTION_STATUS_ENCRYPTED,    // Connection is encrypted.
    SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,  // Non-secure passive
                                                          // content.
    SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,          // Non-secure form
                                                          // target.
    SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,   // Non-secure active
                                                          // content.
    SITE_CONNECTION_STATUS_UNENCRYPTED,       // Connection is not encrypted.
    SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,   // Connection error occurred.
    SITE_CONNECTION_STATUS_INTERNAL_PAGE,     // Internal site.
    SITE_CONNECTION_STATUS_ISOLATED_WEB_APP,  // Isolated Web Apps are either
                                              // from a Signed Web Bundle on
                                              // local filesystem or from a
                                              // trusted developer
                                              // server(dev-proxy mode).
  };

  // Validation status of a website's identity.
  enum SiteIdentityStatus {
    // No status about the website's identity available.
    SITE_IDENTITY_STATUS_UNKNOWN = 0,
    // The website provided a valid certificate.
    SITE_IDENTITY_STATUS_CERT,
    // The website provided a valid EV certificate.
    SITE_IDENTITY_STATUS_EV_CERT,
    // Site identity could not be verified because the site did not provide a
    // certificate. This is the expected state for HTTP connections.
    SITE_IDENTITY_STATUS_NO_CERT,
    // An error occured while verifying the site identity.
    SITE_IDENTITY_STATUS_ERROR,
    // The site is a trusted internal chrome page.
    SITE_IDENTITY_STATUS_INTERNAL_PAGE,
    // The profile has accessed data using an administrator-provided
    // certificate, so the administrator might be able to intercept data.
    SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT,
    // The website provided a valid certificate, but the certificate or chain
    // is using a deprecated signature algorithm.
    SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM,
    // Isolated Web Apps are loaded from local resource (Signed Web Bundle),
    // except when installed in dev-mode-proxy. The identities of Isolated Web
    // Apps are associated with the bundle signature.
    SITE_IDENTITY_STATUS_ISOLATED_WEB_APP,
  };

  // Safe Browsing status of a website.
  enum SafeBrowsingStatus {
    SAFE_BROWSING_STATUS_NONE = 0,
    // The website has been flagged by Safe Browsing as dangerous for
    // containing malware, social engineering, unwanted software, or password
    // reuse on a low reputation site.
    SAFE_BROWSING_STATUS_MALWARE,
    SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING,
    SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE,
    SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE,
    SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
    SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE,
    SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE,
    SAFE_BROWSING_STATUS_BILLING,
    SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN,
    SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK,
  };

  // Events for UMA. Do not reorder or change! Exposed in header so enum is
  // accessible from test.
  enum SSLCertificateDecisionsDidRevoke {
    USER_CERT_DECISIONS_NOT_REVOKED = 0,
    USER_CERT_DECISIONS_REVOKED = 1,
    END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM
  };

  struct ChooserUIInfo {
    ContentSettingsType content_settings_type;
    int description_string_id;
    int allowed_by_policy_description_string_id;
    int delete_tooltip_string_id;
  };

  // |PermissionInfo| contains information about a single permission |type| for
  // the current website.
  struct PermissionInfo {
    PermissionInfo();
    PermissionInfo(const PermissionInfo& other);
    PermissionInfo& operator=(const PermissionInfo& other);
    ~PermissionInfo();

    // Site permission |type|.
    ContentSettingsType type = ContentSettingsType::DEFAULT;
    // The current value for the permission |type| (e.g. ALLOW or BLOCK).
    ContentSetting setting = CONTENT_SETTING_DEFAULT;
    // The global default settings for this permission |type|.
    ContentSetting default_setting = CONTENT_SETTING_DEFAULT;
    // The settings source e.g. user, extensions, policy, ... .
    content_settings::SettingSource source =
        content_settings::SettingSource::kNone;
    // Whether the permission is a one-time grant.
    bool is_one_time = false;
    // Only set for settings that can have multiple permissions for different
    // embedded origins.
    std::optional<url::Origin> requesting_origin;
    // When the permission was used.
    base::Time last_used;
    // Whether the permission is in use.
    bool is_in_use = false;
  };

  // Creates a PageInfo for the passed |url| using the given |ssl| status
  // object to determine the status of the site's connection. Computes the UI
  // inputs and records page info opened action. It is assumed that this is
  // created when page info dialog is opened and destroyed when the dialog is
  // closed.
  PageInfo(std::unique_ptr<PageInfoDelegate> delegate,
           content::WebContents* web_contents,
           const GURL& url);

  PageInfo(const PageInfo&) = delete;
  PageInfo& operator=(const PageInfo&) = delete;

  ~PageInfo() override;

  // Called when the third-party blocking toggle in the cookies subpage gets
  // clicked.
  void OnThirdPartyToggleClicked(bool block_third_party_cookies);

  // Checks whether this permission is currently the factory default, as set by
  // Chrome. Specifically, that the following three conditions are true:
  //   - The current active setting comes from the default or pref provider.
  //   - The setting is the factory default setting (as opposed to a global
  //     default setting set by the user).
  //   - The setting is a wildcard setting applying to all origins (which can
  //     only be set from the default provider).
  static bool IsPermissionFactoryDefault(const PermissionInfo& info,
                                         bool is_incognito);

  // Returns whether this page info is for an internal page.
  static bool IsFileOrInternalPage(const GURL& url);

  // Initializes the current UI and calls present data methods on it to notify
  // the current UI about the data it is subscribed to.
  void InitializeUiState(PageInfoUI* ui, base::OnceClosure done);

  // This method is called to update the presenter's security state and forwards
  // that change on to the UI to be redrawn.
  void UpdateSecurityState();

  void RecordPageInfoAction(page_info::PageInfoAction action);

  void UpdatePermissions();

  // This method is called when ever a permission setting is changed.
  void OnSitePermissionChanged(ContentSettingsType type,
                               ContentSetting value,
                               std::optional<url::Origin> requesting_origin,
                               bool is_one_time);

  // This method is called whenever access to an object is revoked.
  void OnSiteChosenObjectDeleted(const ChooserUIInfo& ui_info,
                                 const base::Value& object);

  // This method is called by the UI when the UI is closing.
  // If specified, |reload_prompt| is set to whether closing the UI resulted in
  // a prompt to the user to reload the page.
  void OnUIClosing(bool* reload_prompt);

  // This method is called when the revoke SSL error bypass button is pressed.
  void OnRevokeSSLErrorBypassButtonPressed();

  // Handles opening the link to show more site settings and records the event.
  void OpenSiteSettingsView();

  // Handles opening the link to show cookies settings and records the event.
  void OpenCookiesSettingsView();

  // Handles opening the link to show all sites settings with a filter for
  // current site's fps  and records the event.
  void OpenAllSitesViewFilteredToRws();

  // Handles opening the cookies dialog and records the event.
  void OpenCookiesDialog();

  // Handles opening the certificate dialog and records the event.
  void OpenCertificateDialog(net::X509Certificate* certificate);

  // Handles opening the safery tip help center page.
  void OpenSafetyTipHelpCenterPage();

  // Handles opening the connection help center page and records the event.
  void OpenConnectionHelpCenterPage(const ui::Event& event);

  // Handles opening the settings page for a permission.
  void OpenContentSettingsExceptions(ContentSettingsType content_settings_type);

  // This method is called when the user pressed "Change password" button.
  void OnChangePasswordButtonPressed();

  // This method is called when the user pressed "Mark as legitimate" button.
  void OnAllowlistPasswordReuseButtonPressed();

  // This method is called when the user opens the Cookies & Site Data subpage.
  void OnCookiesPageOpened();

  // Return a pointer to the ObjectPermissionContextBase corresponding to the
  // content settings type, |type|. Returns nullptr for content settings
  // for which there's no ObjectPermissionContextBase.
  permissions::ObjectPermissionContextBase* GetChooserContextFromUIInfo(
      const ChooserUIInfo& ui_info) const;

  // Accessors.
  const SiteConnectionStatus& site_connection_status() const {
    return site_connection_status_;
  }

  const GURL& site_url() const { return site_url_; }

  const SiteIdentityStatus& site_identity_status() const {
    return site_identity_status_;
  }

  const SafeBrowsingStatus& safe_browsing_status() const {
    return safe_browsing_status_;
  }

  // For most sites, this returns a human-friendly string based on site origin,
  // without scheme, the username and password, the path or trivial subdomains.
  //
  // For Isolated Web Apps & Chrome Extensions, the origin's host name is a
  // non-human-readable string of characters, so instead of displaying the
  // origin, the short name of the app will be displayed.
  std::u16string GetSubjectNameForDisplay() const;

  // Retrieves all the permissions that are shown in Page Info.
  // Exposed for testing.
  static std::vector<ContentSettingsType> GetAllPermissionsForTesting();

  PageInfoUI* ui_for_testing() const { return ui_; }

  void SetSiteNameForTesting(const std::u16string& site_name);

  void SetSubscribedToPermissionChangeForTesting() {
    is_subscribed_to_permission_change_for_testing = true;
  }

  void PresentSitePermissionsForTesting() { PresentSitePermissions(); }

  // PageSpecificContentSettings::PermissionUsageObserver:
  void OnPermissionUsageChange() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PageInfoTest,
                           ShowInfoBarWhenAllowingThirdPartyCookies);
  FRIEND_TEST_ALL_PREFIXES(PageInfoTest,
                           ShowInfoBarWhenBlockingThirdPartyCookies);

  // CookieControlsObserver:
  void OnStatusChanged(bool controls_visible,
                       bool protections_on,
                       CookieControlsEnforcement enforcement,
                       CookieBlocking3pcdStatus blocking_status,
                       base::Time expiration,
                       std::vector<content_settings::TrackingProtectionFeature>
                           features) override;

  // Populates this object's UI state with provided security context. This
  // function does not update visible UI-- that's part of Present*().
  void ComputeUIInputs(const GURL& url);

  // Populates the setting, default_setting, source and is_one_time fields of
  // the |permission_info| struct based on the passed in information as well
  // as the embargo status of the permission. permission_info.type must already
  // be set.
  void PopulatePermissionInfo(PermissionInfo& permission_info,
                              HostContentSettingsMap* content_settings,
                              const content_settings::SettingInfo& info,
                              ContentSetting setting) const;

  // Returns whether |info| should be displayed in the UI.
  bool ShouldShowPermission(const PageInfo::PermissionInfo& info) const;

  // Sets (presents) the information about the site's permissions in the
  // |ui_|.
  void PresentSitePermissions();

  // Helper function which `PresentSiteData` calls after the ignored empty
  // storage keys have been updated.
  void PresentSiteDataInternal(base::OnceClosure done);

  // Sets (presents) the information about the site's data in the |ui_|.
  void PresentSiteData(base::OnceClosure done);

  // Sets (presents) the information about the site's identity and connection
  // in the |ui_|.
  void PresentSiteIdentity();

  // Presents feature related info in the |ui_|; like, if VR content is being
  // presented in a headset.
  void PresentPageFeatureInfo();

  // Sets (presents) the information about ad personalization in the |ui_|.
  void PresentAdPersonalizationData();

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Records a password reuse event. If FULL_SAFE_BROWSING is defined, this
  // function WILL record an event. Callers should check conditions beforehand.
  void RecordPasswordReuseEvent();
#endif

  // Helper function to get the |HostContentSettingsMap| associated with
  // |PageInfo|.
  HostContentSettingsMap* GetContentSettings() const;

  // Helper function to get the Safe Browsing status and details by malicious
  // content status.
  // TODO(jdeblasio): Eliminate this and just use MaliciousContentStatus?
  void GetSafeBrowsingStatusByMaliciousContentStatus(
      security_state::MaliciousContentStatus malicious_content_status,
      PageInfo::SafeBrowsingStatus* status,
      std::u16string* details);

  // Returns PageSpecificContentSettings for the observed WebContents if
  // present, nullptr otherwise.
  content_settings::PageSpecificContentSettings*
  GetPageSpecificContentSettings() const;

  // Whether the content setting of type |type| has changed via Page Info UI.
  bool HasContentSettingChangedViaPageInfo(ContentSettingsType type) const;

  // Notifies the delegate that the content setting of type |type| has changed
  // via Page Info UI.
  void ContentSettingChangedViaPageInfo(ContentSettingsType type);

  // Get the count of blocked and allowed sites.
  int GetSitesWithAllowedCookiesAccessCount();
  int GetThirdPartySitesWithBlockedCookiesAccessCount(const GURL& site_url);

  bool IsIsolatedWebApp() const;

  std::set<net::SchemefulSite> GetTwoSitePermissionRequesters(
      ContentSettingsType type);

  // The page info UI displays information and controls for site-
  // specific data (local stored objects like cookies), site-specific
  // permissions (location, pop-up, plugin, etc. permissions) and site-specific
  // information (identity, connection status, etc.).
  raw_ptr<PageInfoUI, DanglingUntriaged> ui_ = nullptr;

  // A web contents getter used to retrieve the associated WebContents object.
  base::WeakPtr<content::WebContents> web_contents_;

  // The delegate allows the embedder to customize |PageInfo|'s behavior.
  std::unique_ptr<PageInfoDelegate> delegate_;

  // The flag that controls whether an infobar is displayed after the website
  // settings UI is closed or not.
  bool show_info_bar_;

  // The Omnibox URL of the website for which to display site permissions and
  // site information.
  GURL site_url_;

  // Status of the website's identity verification check.
  SiteIdentityStatus site_identity_status_;

  // Safe Browsing status of the website.
  SafeBrowsingStatus safe_browsing_status_;

  // Safety tip info of the website. Set regardless of whether the feature is
  // enabled to show the UI.
  security_state::SafetyTipInfo safety_tip_info_;

  // For secure connection |certificate_| is set to the server certificate.
  scoped_refptr<net::X509Certificate> certificate_;

  // Status of the connection to the website.
  SiteConnectionStatus site_connection_status_;

  // TODO(markusheintz): Move the creation of all the std::u16string typed UI
  // strings below to the corresponding UI code, in order to prevent
  // unnecessary UTF-8 string conversions.

#if BUILDFLAG(IS_ANDROID)
  // Details about the website's identity. If the website's identity has been
  // verified then |identity_status_description_android_| contains who verified
  // the identity. This string will be displayed in the UI.
  std::u16string identity_status_description_android_;
#endif

  // Set when the user has explicitly bypassed an SSL error for this host
  // and/or the user has explicitly bypassed an HTTP warning (from HTTPS-First
  // Mode) for this host. When `show_ssl_decision_revoke_button` is true, the
  // connection area of the page info UI will include an option for the user
  // to revoke their decision to bypass warnings for this host.
  bool show_ssl_decision_revoke_button_;

  // Details about the connection to the website. In case of an encrypted
  // connection |site_connection_details_| contains encryption details, like
  // encryption strength and ssl protocol version. This string will be
  // displayed in the UI.
  std::u16string site_connection_details_;

  // For websites that provided an EV certificate |orgainization_name_|
  // contains the organization name of the certificate. In all other cases
  // |organization_name| is an empty string. This string will be displayed in
  // the UI.
  std::u16string organization_name_;

  bool did_revoke_user_ssl_decisions_;

  security_state::SecurityLevel security_level_;

  security_state::VisibleSecurityState visible_security_state_for_metrics_;

  // Set when the user ignored the password reuse modal warning dialog. When
  // |show_change_password_buttons_| is true, the page identity area of the page
  // info will include buttons to change corresponding password, and to
  // whitelist current site.
  bool show_change_password_buttons_;

  // The time the Page Info UI is opened, for measuring total time open.
  base::TimeTicks start_time_;

  // Records whether the user interacted with the bubble beyond opening it.
  bool did_perform_action_;

  // Description of the Safe Browsing status. Non-empty if
  // MaliciousContentStatus isn't NONE.
  std::u16string safe_browsing_details_;

  std::u16string site_name_for_testing_;

  std::unique_ptr<content_settings::CookieControlsController> controller_;
  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsObserver>
      observation_{this};

  bool protections_on_ = true;
  bool controls_visible_ = true;

  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;

  CookieBlocking3pcdStatus blocking_status_ =
      CookieBlocking3pcdStatus::kNotIn3pcd;

  std::vector<content_settings::TrackingProtectionFeature> features_;

  base::Time cookie_exception_expiration_;

  bool is_subscribed_to_permission_change_for_testing = false;

  base::WeakPtrFactory<PageInfo> weak_factory_{this};
};

#endif  // COMPONENTS_PAGE_INFO_PAGE_INFO_H_
