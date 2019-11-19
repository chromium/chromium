// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/ui/page_info/page_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/buildflags.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_ANDROID)
#include "ui/gfx/image/image_skia.h"
#endif

class GURL;
class Profile;
class PageInfo;

namespace net {
class X509Certificate;
}

// The class |PageInfoUI| specifies the platform independent
// interface of the page info UI. The page info UI displays
// information and controls for site specific data (local stored objects like
// cookies), site specific permissions (location, popup, plugin, etc.
// permissions) and site specific information (identity, connection status,
// etc.).
class PageInfoUI {
 public:
  enum class SecuritySummaryColor {
    RED,
    GREEN,
  };

  enum class SecurityDescriptionType {
    // The UI describes whether the connection is secure, e.g. secure
    // HTTPS, non-secure HTTP.
    CONNECTION,
    // The UI describes e.g. an internal (chrome://) page or extension page.
    INTERNAL,
    // The UI describes a Safe Browsing warning, e.g. site deceptive or contains
    // malware.
    SAFE_BROWSING,
    // The UI shows a Safety Tip.
    SAFETY_TIP,
  };

  struct SecurityDescription {
    // The text style for |summary| used to color it. This provides an
    // opinionated guide to the user on the overall security state of the site.
    SecuritySummaryColor summary_style;
    // A one-line summary of the security state.
    base::string16 summary;
    // A short paragraph with more details about the state, and how
    // the user should treat it.
    base::string16 details;
    // The category of the security description, used to determine which help
    // center article to link to.
    SecurityDescriptionType type;
  };

  // |CookieInfo| contains information about the cookies from a specific source.
  // A source can for example be a specific origin or an entire wildcard domain.
  struct CookieInfo {
    CookieInfo();

    // The number of allowed cookies.
    int allowed;
    // The number of blocked cookies.
    int blocked;

    // Whether these cookies are from the current top-level origin as seen by
    // the user, or from third-party origins.
    bool is_first_party;
  };

  // |PermissionInfo| contains information about a single permission |type| for
  // the current website.
  struct PermissionInfo {
    PermissionInfo();
    // Site permission |type|.
    ContentSettingsType type;
    // The current value for the permission |type| (e.g. ALLOW or BLOCK).
    ContentSetting setting;
    // The global default settings for this permission |type|.
    ContentSetting default_setting;
    // The settings source e.g. user, extensions, policy, ... .
    content_settings::SettingSource source;
    // Whether the profile is off the record.
    bool is_incognito;
  };

  // |ChosenObjectInfo| contains information about a single |chooser_object| of
  // a chooser |type| that the current website has been granted access to.
  struct ChosenObjectInfo {
    ChosenObjectInfo(
        const PageInfo::ChooserUIInfo& ui_info,
        std::unique_ptr<ChooserContextBase::Object> chooser_object);
    ~ChosenObjectInfo();
    // |ui_info| for this chosen object type.
    const PageInfo::ChooserUIInfo& ui_info;
    // The opaque |chooser_object| representing the thing the user selected.
    std::unique_ptr<ChooserContextBase::Object> chooser_object;
  };

  // |IdentityInfo| contains information about the site's identity and
  // connection.
  struct IdentityInfo {
    IdentityInfo();
    ~IdentityInfo();

    // The site's identity: the certificate's Organization Name for sites with
    // Extended Validation certificates, or the URL's hostname for all other
    // sites.
    std::string site_identity;
    // Status of the site's identity.
    PageInfo::SiteIdentityStatus identity_status;
    // Site's Safe Browsing status.
    PageInfo::SafeBrowsingStatus safe_browsing_status;
    // Site's safety tip info. Only set if the feature is enabled to show the
    // Safety Tip UI.
    security_state::SafetyTipInfo safety_tip_info;

#if defined(OS_ANDROID)
    // Textual description of the site's identity status that is displayed to
    // the user.
    std::string identity_status_description_android;
#endif

    // The server certificate if a secure connection.
    scoped_refptr<net::X509Certificate> certificate;
    // Status of the site's connection.
    PageInfo::SiteConnectionStatus connection_status;
    // Textual description of the site's connection status that is displayed to
    // the user.
    std::string connection_status_description;
    // Set when the user has explicitly bypassed an SSL error for this host and
    // has a flag set to remember ssl decisions (explicit flag or in the
    // experimental group).  When |show_ssl_decision_revoke_button| is true, the
    // connection area of the page info will include an option for the user to
    // revoke their decision to bypass the SSL error for this host.
    bool show_ssl_decision_revoke_button;
    // Set when the user ignored the password reuse modal warning dialog. When
    // |show_change_password_buttons| is true, the page identity area of the
    // page info will include buttons to change corresponding password, and
    // to whitelist current site.
    bool show_change_password_buttons;
  };

  struct PageFeatureInfo {
    PageFeatureInfo();

    // True if VR content is being presented in a headset.
    bool is_vr_presentation_in_headset;
  };

  using CookieInfoList = std::vector<CookieInfo>;
  using PermissionInfoList = std::vector<PermissionInfo>;
  using ChosenObjectInfoList = std::vector<std::unique_ptr<ChosenObjectInfo>>;

  virtual ~PageInfoUI();

  // Returns the UI string for the given permission |type|.
  static base::string16 PermissionTypeToUIString(ContentSettingsType type);

  // Returns the UI string describing the action taken for a permission,
  // including why that action was taken. E.g. "Allowed by you",
  // "Blocked by default". If |setting| is default, specify the actual default
  // setting using |default_setting|.
  static base::string16 PermissionActionToUIString(
      Profile* profile,
      ContentSettingsType type,
      ContentSetting setting,
      ContentSetting default_setting,
      content_settings::SettingSource source);

  // Returns a string indicating whether the permission was blocked via an
  // extension, enterprise policy, or embargo.
  static base::string16 PermissionDecisionReasonToUIString(
      Profile* profile,
      const PermissionInfo& permission,
      const GURL& url);

  // Returns the color to use for the permission decision reason strings.
  static SkColor GetSecondaryTextColor();

  // Returns the UI string describing the given object |info|.
  static base::string16 ChosenObjectToUIString(const ChosenObjectInfo& info);

#if defined(OS_ANDROID)
  // Returns the identity icon ID for the given identity |status|.
  static int GetIdentityIconID(PageInfo::SiteIdentityStatus status);

  // Returns the connection icon ID for the given connection |status|.
  static int GetConnectionIconID(PageInfo::SiteConnectionStatus status);
#else  // !defined(OS_ANDROID)
  // Returns icons for the given PermissionInfo |info|. If |info|'s current
  // setting is CONTENT_SETTING_DEFAULT, it will return the icon for |info|'s
  // default setting.
  static const gfx::ImageSkia GetPermissionIcon(
      const PermissionInfo& info,
      const SkColor related_text_color);

  // Returns the icon for the given object |info|.
  static const gfx::ImageSkia GetChosenObjectIcon(
      const ChosenObjectInfo& info,
      bool deleted,
      const SkColor related_text_color);

  // Returns the icon for the page Certificate.
  static const gfx::ImageSkia GetCertificateIcon(
      const SkColor related_text_color);

  // Returns the icon for the button / link to Site settings.
  static const gfx::ImageSkia GetSiteSettingsIcon(
      const SkColor related_text_color);

  // Returns the icon for VR settings.
  static const gfx::ImageSkia GetVrSettingsIcon(SkColor related_text_color);
#endif

  // Return true if the given ContentSettingsType is in PageInfoUI.
  static bool ContentSettingsTypeInPageInfo(ContentSettingsType type);

  static std::unique_ptr<SecurityDescription>
  CreateSafetyTipSecurityDescription(const security_state::SafetyTipInfo& info);

  // Sets cookie information.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) = 0;

  // Sets permission information.
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list,
      ChosenObjectInfoList chosen_object_info_list) = 0;

  // Sets site identity information.
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) = 0;

  virtual void SetPageFeatureInfo(const PageFeatureInfo& page_feature_info) = 0;

  // Helper to get security description info to display to the user.
  std::unique_ptr<SecurityDescription> GetSecurityDescription(
      const IdentityInfo& identity_info) const;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Creates security description for password reuse case.
  virtual std::unique_ptr<SecurityDescription>
  CreateSecurityDescriptionForPasswordReuse() const = 0;
#endif
};

typedef PageInfoUI::CookieInfoList CookieInfoList;
typedef PageInfoUI::PermissionInfoList PermissionInfoList;
typedef PageInfoUI::ChosenObjectInfoList ChosenObjectInfoList;

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_
