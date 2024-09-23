// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_SHORTCUT_INFO_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_SHORTCUT_INFO_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/webapps/browser/android/webapp_icon.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace webapps {

// https://wicg.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
struct ShareTargetParamsFile {
  std::u16string name;
  std::vector<std::u16string> accept;
  ShareTargetParamsFile();
  ShareTargetParamsFile(const ShareTargetParamsFile& other);
  ~ShareTargetParamsFile();
};

// https://wicg.github.io/web-share-target/#dom-sharetargetparams
struct ShareTargetParams {
  std::u16string title;
  std::u16string text;
  std::u16string url;
  std::vector<ShareTargetParamsFile> files;
  ShareTargetParams();
  ShareTargetParams(const ShareTargetParams& other);
  ~ShareTargetParams();
};

// https://wicg.github.io/web-share-target/#dom-sharetarget
struct ShareTarget {
  GURL action;
  blink::mojom::ManifestShareTarget_Method method;
  blink::mojom::ManifestShareTarget_Enctype enctype;
  ShareTargetParams params;
  ShareTarget();
  ~ShareTarget();
};

// Information needed to create a shortcut via ShortcutHelper.
struct ShortcutInfo {
  // Creates a ShortcutInfo struct suitable for adding a shortcut to the home
  // screen.
  static std::unique_ptr<ShortcutInfo> CreateShortcutInfo(
      const GURL& url,
      const GURL& manifest_url,
      const blink::mojom::Manifest& manifest,
      const mojom::WebPageMetadata& web_page_metadata,
      const GURL& primary_icon_url,
      bool primary_icon_maskable);

  // This enum is used to back a UMA histogram, and must be treated as
  // append-only.
  // A Java counterpart will be generated for this enum.
  // Some enum values are duplicated in
  // org.chromium.webapk.lib.common.WebApkConstants.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ShortcutSource
  enum Source {
    SOURCE_UNKNOWN = 0,
    SOURCE_ADD_TO_HOMESCREEN_DEPRECATED = 1,

    // SOURCE_APP_BANNER = 2, (deprecated)

    SOURCE_BOOKMARK_NAVIGATOR_WIDGET = 3,

    // unused
    // SOURCE_BOOKMARK_SHORTCUT_WIDGET = 4,

    // Used for legacy and WebAPKs launched from a notification.
    SOURCE_NOTIFICATION = 5,

    // Used for WebAPKs.
    SOURCE_ADD_TO_HOMESCREEN_PWA = 6,

    // Used for legacy PWAs added via the menu item.
    SOURCE_ADD_TO_HOMESCREEN_STANDALONE = 7,

    // Used for bookmark-type shortcuts that launch the tabbed browser.
    SOURCE_ADD_TO_HOMESCREEN_SHORTCUT = 8,

    // Used for WebAPKs launched via an external intent and not from Chrome.
    SOURCE_EXTERNAL_INTENT = 9,

    // Used for WebAPK PWAs added via the banner.
    // SOURCE_APP_BANNER_WEBAPK = 10, (deprecated)

    // Used for WebAPK PWAs whose install source info was lost.
    SOURCE_WEBAPK_UNKNOWN = 11,

    // Used for Trusted Web Activities launched from third party Android apps.
    // SOURCE_TRUSTED_WEB_ACTIVITY = 12, (deprecated)

    // Used for WebAPK intents received as a result of text sharing events.
    SOURCE_WEBAPK_SHARE_TARGET = 13,

    // Used for WebAPKs launched via an external intent from this Chrome APK.
    // WebAPKs launched from a different Chrome APK (e.g. Chrome Canary) will
    // report SOURCE_EXTERNAL_INTENT.
    SOURCE_EXTERNAL_INTENT_FROM_CHROME = 14,

    // Used for WebAPK intents received as a result of binary file sharing
    // events.
    SOURCE_WEBAPK_SHARE_TARGET_FILE = 15,

    // Used for WebAPKs added by the Chrome Android service after the
    // install was requested by another app.
    // SOURCE_CHROME_SERVICE = 16, (deprecated)

    // SOURCE_INSTALL_RETRY = 17, (deprecated)

    SOURCE_COUNT = 18
  };

  explicit ShortcutInfo(const GURL& shortcut_url);
  ShortcutInfo(const ShortcutInfo& other);
  virtual ~ShortcutInfo();

  // Updates the info based on the given web page metadata.
  void UpdateFromWebPageMetadata(
      const mojom::WebPageMetadata& web_page_metadata);

  // Updates the info based on the given |manifest|.
  void UpdateFromManifest(const blink::mojom::Manifest& manifest);

  // Update the splash screen icon URL based on the given |manifest| for the
  // later download.
  void UpdateBestSplashIcon(const blink::mojom::Manifest& manifest);

  // Update the display mode based on whether the web app is webapk_compatible.
  void UpdateDisplayMode(bool webapk_compatible);

  // Returns a vector of icons including |best_primary_icon_url|,
  // |splash_image_url| and |best_shortcut_icon_urls| if they are not empty
  virtual std::map<GURL, std::unique_ptr<WebappIcon>> GetWebApkIcons() const;

  GURL manifest_url;
  GURL url;
  GURL scope;
  std::u16string user_title;
  std::u16string name;
  std::u16string short_name;
  std::u16string description;
  std::vector<std::u16string> categories;
  blink::mojom::DisplayMode display = blink::mojom::DisplayMode::kBrowser;
  device::mojom::ScreenOrientationLockType orientation =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  std::optional<SkColor> theme_color;
  std::optional<SkColor> background_color;
  int ideal_splash_image_size_in_px = 0;
  int minimum_splash_image_size_in_px = 0;
  GURL best_primary_icon_url;
  bool is_primary_icon_maskable = false;
  GURL splash_image_url;
  bool is_splash_image_maskable = false;
  bool has_custom_title = false;
  std::vector<std::string> icon_urls;
  std::vector<GURL> screenshot_urls;
  std::optional<ShareTarget> share_target;
  std::optional<SkColor> dark_theme_color;
  std::optional<SkColor> dark_background_color;

  // Id specified in the manifest.
  GURL manifest_id;

  // Both shortcut item related vectors have the same size.
  std::vector<blink::Manifest::ShortcutItem> shortcut_items;
  std::vector<GURL> best_shortcut_icon_urls;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_SHORTCUT_INFO_H_
