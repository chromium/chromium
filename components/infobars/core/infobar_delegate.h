// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/base/window_open_disposition.h"

class ConfirmInfoBarDelegate;
class HungRendererInfoBarDelegate;
class InsecureContentInfoBarDelegate;
class NativeAppInfoBarDelegate;
class PopupBlockedInfoBarDelegate;
class RegisterProtocolHandlerInfoBarDelegate;
class ScreenCaptureInfoBarDelegate;
class ThemeInstalledInfoBarDelegate;
class ThreeDAPIInfoBarDelegate;

#if defined(OS_ANDROID)
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

  enum InfoBarAutomationType {
    CONFIRM_INFOBAR,
    PASSWORD_INFOBAR,
    RPH_INFOBAR,
    UNKNOWN_INFOBAR,
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
    APP_BANNER_INFOBAR_DELEGATE = 1,
    // Removed: APP_BANNER_INFOBAR_DELEGATE_DESKTOP = 2,
    // Removed: ANDROID_DOWNLOAD_MANAGER_DUPLICATE_INFOBAR_DELEGATE = 3,
    DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID = 4,
    // Removed: DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID = 5,
    // Removed: FULLSCREEN_INFOBAR_DELEGATE = 6,
    HUNG_PLUGIN_INFOBAR_DELEGATE = 7,
    HUNG_RENDERER_INFOBAR_DELEGATE_ANDROID = 8,
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
    GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID = 25,
    SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE = 26,
    PEPPER_BROKER_INFOBAR_DELEGATE = 27,
    PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID = 28,
    // Removed: DURABLE_STORAGE_PERMISSION_INFOBAR_DELEGATE_ANDROID = 29,
    // Removed: NPAPI_REMOVAL_INFOBAR_DELEGATE = 30,
    OUTDATED_PLUGIN_INFOBAR_DELEGATE = 31,
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
    WINDOW_ERROR_INFOBAR_DELEGATE_ANDROID = 60,
    DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_ANDROID = 61,
    // Removed: DESKTOP_SEARCH_REDIRECTION_INFOBAR_DELEGATE = 62,
    UPDATE_PASSWORD_INFOBAR_DELEGATE_MOBILE = 63,
    DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID = 64,
    AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_ANDROID = 65,
    ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID = 66,
    INSTANT_APPS_INFOBAR_DELEGATE_ANDROID = 67,
    DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE = 68,
    SCREEN_CAPTURE_INFOBAR_DELEGATE_ANDROID = 69,
    GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID = 70,
    OFFLINE_PAGE_INFOBAR_DELEGATE_ANDROID = 71,
    SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_ANDROID = 72,
    AUTOMATION_INFOBAR_DELEGATE = 73,
    VR_SERVICES_UPGRADE_ANDROID = 74,
    READER_MODE_INFOBAR_ANDROID = 75,
    VR_FEEDBACK_INFOBAR_ANDROID = 76,
    FRAMEBUST_BLOCK_INFOBAR_ANDROID = 77,
    SURVEY_INFOBAR_ANDROID = 78,
    NEAR_OOM_INFOBAR_ANDROID = 79,
    INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE = 80,
    PAGE_LOAD_CAPPING_INFOBAR_DELEGATE = 81,
    DOWNLOAD_PROGRESS_INFOBAR_ANDROID = 82,
    AR_CORE_UPGRADE_ANDROID = 83,
    BLOATED_RENDERER_INFOBAR_DELEGATE = 84,
    SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE = 85,
    NEAR_OOM_REDUCTION_INFOBAR_ANDROID = 86,
    LITE_PAGE_PREVIEWS_INFOBAR = 87,
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
  };

  // Value to use when the InfoBar has no icon to show.
  static const int kNoIconID;

  // Called when the InfoBar that owns this delegate is being destroyed.  At
  // this point nothing is visible onscreen.
  virtual ~InfoBarDelegate();

  // Returns a unique value identifying the infobar.
  // New implementers must append a new value to the InfoBarIdentifier enum here
  // and in histograms.xml.
  virtual InfoBarIdentifier GetIdentifier() const = 0;

  virtual InfoBarAutomationType GetInfoBarAutomationType() const;

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
  virtual gfx::Image GetIcon() const;

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

  // Called when the user clicks on the close button to dismiss the infobar.
  virtual void InfoBarDismissed();

  // Type-checking downcast routines:
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();
  virtual HungRendererInfoBarDelegate* AsHungRendererInfoBarDelegate();
  virtual InsecureContentInfoBarDelegate* AsInsecureContentInfoBarDelegate();
  virtual NativeAppInfoBarDelegate* AsNativeAppInfoBarDelegate();
  virtual PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate();
  virtual RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate();
  virtual ScreenCaptureInfoBarDelegate* AsScreenCaptureInfoBarDelegate();
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual ThreeDAPIInfoBarDelegate* AsThreeDAPIInfoBarDelegate();
  virtual translate::TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();
#if defined(OS_ANDROID)
  virtual offline_pages::OfflinePageInfoBarDelegate*
  AsOfflinePageInfoBarDelegate();
#endif

  void set_infobar(InfoBar* infobar) { infobar_ = infobar; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

 protected:
  InfoBarDelegate();

  InfoBar* infobar() { return infobar_; }

 private:
  // The InfoBar associated with us.
  InfoBar* infobar_;

  // The ID of the active navigation entry at the time we became owned.
  int nav_entry_id_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarDelegate);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_DELEGATE_H_
