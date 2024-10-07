// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class ConfirmInfoBarDelegate;
class ThemeInstalledInfoBarDelegate;

namespace blocked_content {
class PopupBlockedInfoBarDelegate;
}

#if BUILDFLAG(IS_ANDROID)
namespace offline_pages {
class OfflinePageInfoBarDelegate;
}
#endif

namespace translate {
class TranslateInfoBarDelegate;
}

namespace gfx {
class Image;
struct VectorIcon;
}

namespace ui {
class ImageModel;
}

namespace infobars {

class InfoBar;

// An interface implemented by objects wishing to control an InfoBar.
// Implementing this interface is not sufficient to use an InfoBar, since it
// does not map to a specific InfoBar type. Instead, you must implement
// ConfirmInfoBarDelegate, or override with your own delegate for your own
// InfoBar variety.
class InfoBarDelegate {
 public:
  // The type of the infobar. It controls its appearance, such as its background
  // color.
  enum Type {
    WARNING_TYPE,
    PAGE_ACTION_TYPE,
  };

  // Unique identifier for every InfoBarDelegate subclass.  Use suffixes to mark
  // infobars specific to particular OSes/platforms.
  // KEEP IN SYNC WITH THE InfoBarIdentifier ENUM IN enums.xml.
  // NEW VALUES MUST BE APPENDED AND AVOID CHANGING ANY PRE-EXISTING VALUES.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.infobar
  enum InfoBarIdentifier {
    INVALID = -1,
    TEST_INFOBAR = 0,
    // Removed: APP_BANNER_INFOBAR_DELEGATE = 1,
    // Removed: APP_BANNER_INFOBAR_DELEGATE_DESKTOP = 2,
    // Removed: ANDROID_DOWNLOAD_MANAGER_DUPLICATE_INFOBAR_DELEGATE = 3,
    DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID = 4,
    // Removed: DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID = 5,
    // Removed: FULLSCREEN_INFOBAR_DELEGATE = 6,
    HUNG_PLUGIN_INFOBAR_DELEGATE = 7,
    // Removed: HUNG_RENDERER_INFOBAR_DELEGATE_ANDROID = 8,
    // Removed: MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID = 9,
    // Removed: MEDIA_THROTTLE_INFOBAR_DELEGATE = 10,
    // Removed: REQUEST_QUOTA_INFOBAR_DELEGATE = 11,
    DEV_TOOLS_INFOBAR_DELEGATE = 12,
    EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE = 13,
    INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE = 14,
    THEME_INSTALLED_INFOBAR_DELEGATE = 15,
    // Removed: GEOLOCATION_INFOBAR_DELEGATE_ANDROID = 16,
    THREE_D_API_INFOBAR_DELEGATE = 17,
    // Removed: INSECURE_CONTENT_INFOBAR_DELEGATE = 18,
    // Removed: MIDI_PERMISSION_INFOBAR_DELEGATE_ANDROID = 19,
    // Removed: PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID = 20,
    NACL_INFOBAR_DELEGATE = 21,
    // Removed: DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_ANDROID = 22,
    // Removed: NOTIFICATION_PERMISSION_INFOBAR_DELEGATE = 23,
    // Removed: AUTO_SIGNIN_FIRST_RUN_INFOBAR_DELEGATE = 24,
    // Removed: GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID = 25,
    SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE = 26,
    // Removed: PEPPER_BROKER_INFOBAR_DELEGATE = 27,
    // Removed: PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID = 28,
    // Removed: DURABLE_STORAGE_PERMISSION_INFOBAR_DELEGATE_ANDROID = 29,
    // Removed: NPAPI_REMOVAL_INFOBAR_DELEGATE = 30,
    // Removed: OUTDATED_PLUGIN_INFOBAR_DELEGATE = 31,
    // Removed: PLUGIN_METRO_MODE_INFOBAR_DELEGATE = 32,
    RELOAD_PLUGIN_INFOBAR_DELEGATE = 33,
    PLUGIN_OBSERVER_INFOBAR_DELEGATE = 34,
    // Removed: SSL_ADD_CERTIFICATE = 35,
    // Removed: SSL_ADD_CERTIFICATE_INFOBAR_DELEGATE = 36,
    POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE = 37,
    FILE_ACCESS_DISABLED_INFOBAR_DELEGATE = 38,
    KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC = 39,
    COLLECTED_COOKIES_INFOBAR_DELEGATE = 40,
    INSTALLATION_ERROR_INFOBAR_DELEGATE = 41,
    ALTERNATE_NAV_INFOBAR_DELEGATE = 42,
    BAD_FLAGS_INFOBAR_DELEGATE = 43,
    DEFAULT_BROWSER_INFOBAR_DELEGATE = 44,
    GOOGLE_API_KEYS_INFOBAR_DELEGATE = 45,
    OBSOLETE_SYSTEM_INFOBAR_DELEGATE = 46,
    SESSION_CRASHED_INFOBAR_DELEGATE_IOS = 47,
    PAGE_INFO_INFOBAR_DELEGATE = 48,
    AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE = 49,
    TRANSLATE_INFOBAR_DELEGATE_NON_AURA = 50,
    // Removed: IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE = 51,
    // Removed: NATIVE_APP_INSTALLER_INFOBAR_DELEGATE = 52,
    // Removed: NATIVE_APP_LAUNCHER_INFOBAR_DELEGATE = 53,
    // Removed: NATIVE_APP_OPEN_POLICY_INFOBAR_DELEGATE = 54,
    RE_SIGN_IN_INFOBAR_DELEGATE_IOS = 55,
    SHOW_PASSKIT_ERROR_INFOBAR_DELEGATE_IOS = 56,
    // Removed: READER_MODE_INFOBAR_DELEGATE_IOS = 57,
    SYNC_ERROR_INFOBAR_DELEGATE_IOS = 58,
    UPGRADE_INFOBAR_DELEGATE_IOS = 59,
    // Removed: WINDOW_ERROR_INFOBAR_DELEGATE_ANDROID = 60,
    DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_ANDROID = 61,
    // Removed: DESKTOP_SEARCH_REDIRECTION_INFOBAR_DELEGATE = 62,
    // Removed: UPDATE_PASSWORD_INFOBAR_DELEGATE_MOBILE = 63,
    // Removed: DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID = 64,
    // Removed: AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_ANDROID = 65,
    ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID = 66,
    // Removed: INSTANT_APPS_INFOBAR_DELEGATE_ANDROID = 67,
    // Removed: DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE = 68,
    // Removed: SCREEN_CAPTURE_INFOBAR_DELEGATE_ANDROID = 69,
    PERMISSION_INFOBAR_DELEGATE_ANDROID = 70,
    OFFLINE_PAGE_INFOBAR_DELEGATE_ANDROID = 71,
    SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_ANDROID = 72,
    AUTOMATION_INFOBAR_DELEGATE = 73,
    // Removed: VR_SERVICES_UPGRADE_ANDROID = 74,
    // Removed: READER_MODE_INFOBAR_ANDROID = 75,
    // Removed: VR_FEEDBACK_INFOBAR_ANDROID = 76,
    // Removed: FRAMEBUST_BLOCK_INFOBAR_ANDROID = 77,
    // Removed: SURVEY_INFOBAR_ANDROID = 78,
    NEAR_OOM_INFOBAR_ANDROID = 79,
    INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE = 80,
    // Removed: PAGE_LOAD_CAPPING_INFOBAR_DELEGATE = 81,
    DOWNLOAD_PROGRESS_INFOBAR_ANDROID = 82,
    // Removed: AR_CORE_UPGRADE_ANDROID = 83,
    BLOATED_RENDERER_INFOBAR_DELEGATE = 84,
    // Removed: SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE = 85,
    // Removed: NEAR_OOM_REDUCTION_INFOBAR_ANDROID = 86,
    // Removed: LITE_PAGE_PREVIEWS_INFOBAR = 87,
    // Removed: MODULE_INSTALL_FAILURE_INFOBAR_ANDROID = 88,
    // Removed: INLINE_UPDATE_READY_INFOBAR_ANDROID = 89,
    // Removed: INLINE_UPDATE_FAILED_INFOBAR_ANDROID = 90,
    // Removed: FLASH_DEPRECATION_INFOBAR_DELEGATE = 91,
    SEND_TAB_TO_SELF_INFOBAR_DELEGATE = 92,
    TAB_SHARING_INFOBAR_DELEGATE = 93,
    SAFETY_TIP_INFOBAR_DELEGATE = 94,
    WEBOTP_SERVICE_INFOBAR_DELEGATE = 95,
    KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE = 96,
    // Removed: SYNC_ERROR_INFOBAR_DELEGATE_ANDROID = 97,
    INSECURE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID = 98,
    // Removed: CONDITIONAL_TAB_STRIP_INFOBAR_ANDROID = 99,
    // Removed: LITE_MODE_HTTPS_IMAGE_COMPRESSION_INFOBAR_ANDROID = 100,
    // Removed: SYSTEM_INFOBAR_DELEGATE_MAC = 101,
    // Removed: EXPERIMENTAL_INFOBAR_DELEGATE_LACROS = 102,
    // Removed: ROSETTA_REQUIRED_INFOBAR_DELEGATE = 103,
    // Removed: WEBID_PERMISSION_INFOBAR_DELEGATE = 104,
    // Removed: AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE = 105,
    AUTOFILL_ADDRESS_PROFILE_INFOBAR_DELEGATE_IOS = 106,
    ADD_TO_READING_LIST_IOS = 107,
    IOS_PERMISSIONS_INFOBAR_DELEGATE = 108,
    // Removed: SUPPORTED_LINKS_INFOBAR_DELEGATE_CHROMEOS = 109,
    AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE = 110,
    TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE = 111,
    CHROME_FOR_TESTING_INFOBAR_DELEGATE = 112,
    EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE = 113,
    TAB_PICKUP_INFOBAR_DELEGATE = 114,
    LOCAL_TEST_POLICIES_APPLIED_INFOBAR = 115,
    BIDDING_AND_AUCTION_CONSENTED_DEBUGGING_DELEGATE = 116,
    PARCEL_TRACKING_INFOBAR_DELEGATE = 117,
    TEST_THIRD_PARTY_COOKIE_PHASEOUT_DELEGATE = 118,
    ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE = 119,
    DEV_TOOLS_SHARED_PROCESS_DELEGATE = 120,
    ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE = 121,
  };

  // Describes navigation events, used to decide whether infobars should be
  // dismissed.
  struct NavigationDetails {
    // Unique identifier for the entry.
    int entry_id;
    // True if it is a navigation to a different page (as opposed to in-page).
    bool is_navigation_to_different_page;
    // True if the entry replaced the existing one.
    bool did_replace_entry;
    bool is_reload;
    bool is_redirect;
#if BUILDFLAG(IS_IOS)
    // True if the navigation was caused by a form submission.
    bool is_form_submission;
    // True if the navigation was caused by a user gesture, e.g. reload or load
    // new content from the omnibox.
    bool has_user_gesture;
#endif  // BUILDFLAG(IS_IOS)
  };

  // Value to use when the InfoBar has no icon to show.
  static const int kNoIconID;

  InfoBarDelegate(const InfoBarDelegate&) = delete;
  InfoBarDelegate& operator=(const InfoBarDelegate&) = delete;

  // Called when the InfoBar that owns this delegate is being destroyed.  At
  // this point nothing is visible onscreen.
  virtual ~InfoBarDelegate();

  // Returns a unique value identifying the infobar.
  // New implementers must append a new value to the InfoBarIdentifier enum here
  // and in histograms/enums.xml.
  virtual InfoBarIdentifier GetIdentifier() const = 0;

  // Returns the resource ID of the icon to be shown for this InfoBar.  If the
  // value is equal to |kNoIconID|, GetIcon() will not show an icon by default.
  virtual int GetIconId() const;

  // Returns the vector icon identifier to be shown for this InfoBar. This will
  // take precedence over GetIconId() (although typically only one of the two
  // should be defined for any given infobar).
  virtual const gfx::VectorIcon& GetVectorIcon() const;

  // Returns the icon to be shown for this InfoBar. If the returned Image is
  // empty, no icon is shown.
  //
  // Most subclasses should not override this; override GetIconId() instead
  // unless the infobar needs to show an image from somewhere other than the
  // resource bundle as its icon.
  virtual ui::ImageModel GetIcon() const;

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // an empty string.
  virtual std::u16string GetLinkText() const;

  // Returns the URL the link should navigate to.
  virtual GURL GetLinkURL() const;

  // Returns true if the supplied |delegate| is equal to this one. Equality is
  // left to the implementation to define. This function is called by the
  // InfoBarManager when determining whether or not a delegate should be
  // added because a matching one already exists. If this function returns true,
  // the InfoBarManager will not add the new delegate because it considers
  // one to already be present.
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;

  // Returns true if the InfoBar should be closed automatically after the page
  // is navigated. By default this returns true if the navigation is to a new
  // page (not including reloads).  Subclasses wishing to change this behavior
  // can override this function.
  virtual bool ShouldExpire(const NavigationDetails& details) const;

  // Called when the link (if any) is clicked; if this function returns true,
  // the infobar is then immediately closed. The default implementation opens
  // the URL returned by GetLinkURL(), above, and returns false. Subclasses MUST
  // NOT return true if in handling this call something triggers the infobar to
  // begin closing.
  //
  // The |disposition| specifies how the resulting document should be loaded
  // (based on the event flags present when the link was clicked).
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  // Called when the user clicks on the close button to dismiss the infobar.
  virtual void InfoBarDismissed();

  // Returns true if the InfoBar has a close button; true by default.
  virtual bool IsCloseable() const;

  // Returns true if the InfoBar should animate when showing or hiding; true by
  // default.
  virtual bool ShouldAnimate() const;

  // Type-checking downcast routines:
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();
  virtual blocked_content::PopupBlockedInfoBarDelegate*
  AsPopupBlockedInfoBarDelegate();
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
#if BUILDFLAG(IS_IOS)
  virtual translate::TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();
#endif
#if BUILDFLAG(IS_ANDROID)
  virtual offline_pages::OfflinePageInfoBarDelegate*
  AsOfflinePageInfoBarDelegate();
#endif

  void set_infobar(InfoBar* infobar) { infobar_ = infobar; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }
  void set_dark_mode(bool dark_mode) { dark_mode_ = dark_mode; }

 protected:
  InfoBarDelegate();

  InfoBar* infobar() { return infobar_; }

  bool dark_mode() const { return dark_mode_; }

 private:
  // The InfoBar associated with us.
  raw_ptr<InfoBar> infobar_ = nullptr;

  // The ID of the active navigation entry at the time we became owned.
  int nav_entry_id_ = 0;

  // Whether the background of the InfoBar is dark. Normally, this is a UI-level
  // concern that delegates need not worry about. However, some delegates need
  // to change their behavior in this case, e.g. by returning a different icon
  // entirely, not just one with a different color.
  bool dark_mode_ = false;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
