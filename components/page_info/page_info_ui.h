// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_PAGE_INFO_UI_H_
#define COMPONENTS_PAGE_INFO_PAGE_INFO_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/page_info/page_info.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/safe_browsing/buildflags.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/gfx/image/image_skia.h"
#endif

class PageInfo;
class PageInfoUiDelegate;

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
  // Specifies security icons and sections shown for the page info UI. For
  // ENTERPRISE, a red business icon is shown in the omnibox.
  enum class SecuritySummaryColor { RED, GREEN, ENTERPRISE };

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
    std::u16string summary;
    // A short paragraph with more details about the state, and how
    // the user should treat it.
    std::u16string details;
    // The category of the security description, used to determine which help
    // center article to link to.
    SecurityDescriptionType type;
  };

  // `CookiesRwsInfo` contains information about a specific Related website set.
  struct CookiesRwsInfo {
    explicit CookiesRwsInfo(const std::u16string& owner_name);
    ~CookiesRwsInfo();

    // The name of the owner of the RWS.
    std::u16string owner_name;

    // Whether the Rws are managed by the company.
    bool is_managed = false;
  };

  // `CookiesNewInfo` contains information about the sites that are allowed
  // to access cookies and rws cookies info for new UI.
  // TODO(crbug.com/40854087):  Change the name to "CookieInfo" after finishing
  // cookies subpage implementation
  struct CookiesNewInfo {
    CookiesNewInfo();
    CookiesNewInfo(CookiesNewInfo&&);
    ~CookiesNewInfo();

    // The number of third-party sites blocked.
    int blocked_third_party_sites_count = -1;

    // The number of third-party sites allowed.
    int allowed_third_party_sites_count = -1;

    // The number of sites allowed to access cookies.
    int allowed_sites_count = -1;

    // Whether protections are enabled for the given site.
    bool protections_on = true;

    // Whether tracking protection controls should be shown.
    bool controls_visible = true;

    // The type of third-party cookie blocking in 3PCD.
    CookieBlocking3pcdStatus blocking_status =
        CookieBlocking3pcdStatus::kNotIn3pcd;

    // The status of enforcement of blocking third-party cookies.
    CookieControlsEnforcement enforcement;

    // List of ACT features.
    std::vector<content_settings::TrackingProtectionFeature> features;

    std::optional<CookiesRwsInfo> rws_info;

    // The expiration of the active third-party cookie exception.
    base::Time expiration;

    // Whether the current profile is "off the record".
    bool is_otr = false;
  };

  // |ChosenObjectInfo| contains information about a single |chooser_object| of
  // a chooser |type| that the current website has been granted access to.
  struct ChosenObjectInfo {
    ChosenObjectInfo(
        const PageInfo::ChooserUIInfo& ui_info,
        std::unique_ptr<permissions::ObjectPermissionContextBase::Object>
            chooser_object);
    ~ChosenObjectInfo();
    // |ui_info| for this chosen object type.
    const raw_ref<const PageInfo::ChooserUIInfo> ui_info;
    // The opaque |chooser_object| representing the thing the user selected.
    std::unique_ptr<permissions::ObjectPermissionContextBase::Object>
        chooser_object;
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
    // Textual description of the Safe Browsing status.
    std::u16string safe_browsing_details;

#if BUILDFLAG(IS_ANDROID)
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
    // Set when the user has explicitly bypassed an SSL error for this host
    // and/or the user has explicitly bypassed an HTTP warning (from HTTPS-First
    // Mode) for this host. When `show_ssl_decision_revoke_button` is true, the
    // connection area of the page info UI will include an option for the user
    // to revoke their decision to bypass warnings for this host.
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

  struct PermissionUIInfo {
    ContentSettingsType type;
    int string_id;
    int string_id_mid_sentence;
  };

  struct AdPersonalizationInfo {
    AdPersonalizationInfo();
    ~AdPersonalizationInfo();
    bool is_empty() const;

    bool has_joined_user_to_interest_group;
    std::vector<privacy_sandbox::CanonicalTopic> accessed_topics;
  };

  using PermissionInfoList = std::vector<PageInfo::PermissionInfo>;
  using ChosenObjectInfoList = std::vector<std::unique_ptr<ChosenObjectInfo>>;

  virtual ~PageInfoUI();

  // Returns the UI string for the given permission |type|.
  static std::u16string PermissionTypeToUIString(ContentSettingsType type);
  // Returns the UI string for the given permission |type| when used
  // mid-sentence.
  static std::u16string PermissionTypeToUIStringMidSentence(
      ContentSettingsType type);
  // Returns a tooltip for permission |type|.
  static std::u16string PermissionTooltipUiString(
      ContentSettingsType type,
      const std::optional<url::Origin>& requesting_origin);
  // Returns a tooltip for a subpage button for permission |type|.
  static std::u16string PermissionSubpageButtonTooltipString(
      ContentSettingsType type);

  static base::span<const PermissionUIInfo>
  GetContentSettingsUIInfoForTesting();

  // Returns the UI string describing the action taken for a permission,
  // including why that action was taken. E.g. "Allowed by you",
  // "Blocked by default". If |setting| is default, specify the actual default
  // setting using |default_setting|.
  static std::u16string PermissionActionToUIString(
      PageInfoUiDelegate* delegate,
      ContentSettingsType type,
      ContentSetting setting,
      ContentSetting default_setting,
      content_settings::SettingSource source,
      bool is_one_time);

  static std::u16string PermissionStateToUIString(
      PageInfoUiDelegate* delegate,
      const PageInfo::PermissionInfo& permission);

  static std::u16string PermissionMainPageStateToUIString(
      PageInfoUiDelegate* delegate,
      const PageInfo::PermissionInfo& permission);

  static std::u16string PermissionManagedTooltipToUIString(
      PageInfoUiDelegate* delegate,
      const PageInfo::PermissionInfo& permission);

  static std::u16string PermissionAutoBlockedToUIString(
      PageInfoUiDelegate* delegate,
      const PageInfo::PermissionInfo& permission);

  static void ToggleBetweenAllowAndBlock(PageInfo::PermissionInfo& permission);

  static void ToggleBetweenRememberAndForget(
      PageInfo::PermissionInfo& permission);

  static bool IsToggleOn(const PageInfo::PermissionInfo& permission);

  // Returns the color to use for the permission decision reason strings.
  static SkColor GetSecondaryTextColor();

#if BUILDFLAG(IS_ANDROID)
  // Returns the identity icon ID for the given identity |status|.
  static int GetIdentityIconID(PageInfo::SiteIdentityStatus status);

  // Returns the connection icon ID for the given connection |status|.
  static int GetConnectionIconID(PageInfo::SiteConnectionStatus status);

  // Returns the identity icon color ID for the given identity |status|.
  static int GetIdentityIconColorID(PageInfo::SiteIdentityStatus status);

  // Returns the connection icon color ID for the given connection |status|.
  static int GetConnectionIconColorID(PageInfo::SiteConnectionStatus status);
#endif  // BUILDFLAG(IS_ANDROID)

  // Return true if the given ContentSettingsType is in PageInfoUI.
  static bool ContentSettingsTypeInPageInfo(ContentSettingsType type);

  static std::unique_ptr<SecurityDescription>
  CreateSafetyTipSecurityDescription(const security_state::SafetyTipInfo& info);

  // Sets cookie information.
  virtual void SetCookieInfo(const CookiesNewInfo& cookie_info) {}

  // Sets permission information.
  virtual void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                                 ChosenObjectInfoList chosen_object_info_list) {
  }

  // Sets site identity information.
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) {}

  // Sets feature related information; for now only if VR content is being
  // presented in a headset.
  virtual void SetPageFeatureInfo(const PageFeatureInfo& page_feature_info) {}

  // Sets ad personalization information.
  virtual void SetAdPersonalizationInfo(
      const AdPersonalizationInfo& ad_personalization_info) {}

  // Helper to get security description info to display to the user.
  std::unique_ptr<SecurityDescription> GetSecurityDescription(
      const IdentityInfo& identity_info) const;
};

typedef PageInfoUI::PermissionInfoList PermissionInfoList;
typedef PageInfoUI::ChosenObjectInfoList ChosenObjectInfoList;

#endif  // COMPONENTS_PAGE_INFO_PAGE_INFO_UI_H_
