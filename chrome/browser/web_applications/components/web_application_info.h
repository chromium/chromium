// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APPLICATION_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APPLICATION_INFO_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using SquareSizePx = int;
// Iterates in ascending order (checked in SortedSizesPxIsAscending test).
using SortedSizesPx = base::flat_set<SquareSizePx, std::less<>>;
using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

// Icon bitmaps for each IconPurpose.
struct IconBitmaps {
  IconBitmaps();
  ~IconBitmaps();
  IconBitmaps(const IconBitmaps&);
  IconBitmaps(IconBitmaps&&) noexcept;
  IconBitmaps& operator=(const IconBitmaps&);
  IconBitmaps& operator=(IconBitmaps&&) noexcept;

  const std::map<SquareSizePx, SkBitmap>& GetBitmapsForPurpose(
      IconPurpose purpose) const;
  void SetBitmapsForPurpose(IconPurpose purpose,
                            std::map<SquareSizePx, SkBitmap> bitmaps);

  bool empty() const;

  // TODO(crbug.com/1152661): Consider using base::flat_map.

  // Icon bitmaps suitable for any context, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::map<SquareSizePx, SkBitmap> any;

  // Icon bitmaps designed for masking, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::map<SquareSizePx, SkBitmap> maskable;

  // TODO (crbug.com/1114638): Monochrome support.
};

// Icon sizes for each IconPurpose.
struct IconSizes {
  IconSizes();
  ~IconSizes();
  IconSizes(const IconSizes&);
  IconSizes(IconSizes&&) noexcept;
  IconSizes& operator=(const IconSizes&);
  IconSizes& operator=(IconSizes&&) noexcept;

  const std::vector<SquareSizePx>& GetSizesForPurpose(
      IconPurpose purpose) const;
  void SetSizesForPurpose(IconPurpose purpose, std::vector<SquareSizePx> sizes);

  bool empty() const;

  // Sizes of icon bitmaps suitable for any context.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::vector<SquareSizePx> any;

  // Sizes of icon bitmaps designed for masking.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::vector<SquareSizePx> maskable;

  // TODO (crbug.com/1114638): Monochrome support.
};

using ShortcutsMenuIconBitmaps = std::vector<IconBitmaps>;

// TODO(https://crbug.com/1091473): Rename WebApplication* occurrences in this
// file to WebApp*.
struct WebApplicationIconInfo {
  WebApplicationIconInfo();
  WebApplicationIconInfo(const GURL& url, SquareSizePx size);
  WebApplicationIconInfo(const WebApplicationIconInfo&);
  WebApplicationIconInfo(WebApplicationIconInfo&&) noexcept;
  ~WebApplicationIconInfo();
  WebApplicationIconInfo& operator=(const WebApplicationIconInfo&);
  WebApplicationIconInfo& operator=(WebApplicationIconInfo&&) noexcept;

  GURL url;
  base::Optional<SquareSizePx> square_size_px;
  // TODO (crbug.com/1114638): Support Monochrome.
  IconPurpose purpose = IconPurpose::ANY;
};

// Structure used when creating app icon shortcuts menu and for downloading
// associated shortcut icons when supported by OS platform (eg. Windows).
struct WebApplicationShortcutsMenuItemInfo {
  struct Icon {
    Icon();
    Icon(const Icon&);
    Icon(Icon&&);
    ~Icon();
    Icon& operator=(const Icon&);
    Icon& operator=(Icon&&);

    GURL url;
    SquareSizePx square_size_px = 0;
  };

  WebApplicationShortcutsMenuItemInfo();
  WebApplicationShortcutsMenuItemInfo(
      const WebApplicationShortcutsMenuItemInfo&);
  WebApplicationShortcutsMenuItemInfo(
      WebApplicationShortcutsMenuItemInfo&&) noexcept;
  ~WebApplicationShortcutsMenuItemInfo();
  WebApplicationShortcutsMenuItemInfo& operator=(
      const WebApplicationShortcutsMenuItemInfo&);
  WebApplicationShortcutsMenuItemInfo& operator=(
      WebApplicationShortcutsMenuItemInfo&&) noexcept;

  const std::vector<Icon>& GetShortcutIconInfosForPurpose(
      IconPurpose purpose) const;
  void SetShortcutIconInfosForPurpose(IconPurpose purpose,
                                      std::vector<Icon> shortcut_icon_infos);

  // Title of shortcut item in App Icon Shortcut Menu.
  std::u16string name;

  // URL launched when shortcut item is selected.
  GURL url;

  // List of shortcut icon URLs with associated square size,
  // suitable for any context.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::vector<Icon> any;

  // List of shortcut icon URLs with associated square size,
  // designed for masking.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::vector<Icon> maskable;
};

// Structure used when installing a web page as an app.
struct WebApplicationInfo {
  enum MobileCapable {
    MOBILE_CAPABLE_UNSPECIFIED,
    MOBILE_CAPABLE,
    MOBILE_CAPABLE_APPLE
  };

  WebApplicationInfo();
  WebApplicationInfo(const WebApplicationInfo& other);
  explicit WebApplicationInfo(const webapps::mojom::WebPageMetadata& metadata);
  ~WebApplicationInfo();

  // Id specified in the manifest.
  base::Optional<std::string> manifest_id;

  // Title of the application.
  std::u16string title;

  // Description of the application.
  std::u16string description;

  // The URL the site would prefer the user agent load when launching the app.
  // https://www.w3.org/TR/appmanifest/#start_url-member
  GURL start_url;

  // The URL of the manifest.
  // https://www.w3.org/TR/appmanifest/#web-application-manifest
  GURL manifest_url;

  // Optional query parameters to add to the start_url when launching the app.
  base::Optional<std::string> launch_query_params;

  // Scope for the app. Dictates what URLs will be opened in the app.
  // https://www.w3.org/TR/appmanifest/#scope-member
  GURL scope;

  // List of icon URLs with associated square size and purpose.
  std::vector<WebApplicationIconInfo> icon_infos;

  // Icon bitmaps, keyed by their square size.
  IconBitmaps icon_bitmaps;

  // Represents whether the icons for the web app are generated by Chrome due to
  // no suitable icons being available.
  bool is_generated_icon = false;

  // Whether the page is marked as mobile-capable, including apple specific meta
  // tag.
  MobileCapable mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;

  // The color to use if an icon needs to be generated for the web app.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;

  // The color to use for the web app frame.
  base::Optional<SkColor> theme_color;

  // The expected page background color of the web app.
  // https://www.w3.org/TR/appmanifest/#background_color-member
  base::Optional<SkColor> background_color;

  // App preference regarding whether the app should be opened in a tab,
  // in a window (with or without minimal-ui buttons), or full screen. Defaults
  // to browser display mode as specified in
  // https://w3c.github.io/manifest/#display-modes
  blink::mojom::DisplayMode display_mode = blink::mojom::DisplayMode::kBrowser;

  // App preference to control display fallback ordering
  std::vector<blink::mojom::DisplayMode> display_override;

  // User preference as to whether the app should be opened in a window.
  // If false, the app will be opened in a tab.
  // If true, the app will be opened in a window, with minimal-ui buttons
  // if display_mode is kBrowser or kMinimalUi.
  bool open_as_window = false;

  // Whether standalone app windows should have a tab strip. Currently a user
  // preference for the sake of experimental exploration.
  bool enable_experimental_tabbed_window = false;

  // The extensions and mime types the app can handle.
  std::vector<blink::Manifest::FileHandler> file_handlers;

  // File types the app accepts as a Web Share Target.
  base::Optional<apps::ShareTarget> share_target;

  // Additional search terms that users can use to find the app.
  std::vector<std::string> additional_search_terms;

  // Set of shortcuts menu item infos populated using shortcuts specified in the
  // manifest.
  std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos;

  // Vector of shortcut icon bitmaps keyed by their square size. The index of a
  // given |IconBitmaps| matches that of the shortcut in
  // |shortcuts_menu_item_infos| whose bitmaps it contains.
  ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps;

  // The URL protocols/schemes that the app can handle.
  std::vector<blink::Manifest::ProtocolHandler> protocol_handlers;

  // URL within scope to launch for a "new note" action. Valid iff this is
  // considered a note-taking app.
  // TODO(crbug.com/1185678): Parse this from the manifest.
  GURL note_taking_new_note_url;

  // The app intends to act as a URL handler for URLs described by this
  // information.
  apps::UrlHandlers url_handlers;

  // User preference as to whether to auto run the app on OS login.
  // Currently only supported in Windows platform.
  bool run_on_os_login = false;

  // The link capturing behaviour to use for navigations into in the app's
  // scope.
  blink::mojom::CaptureLinks capture_links =
      blink::mojom::CaptureLinks::kUndefined;
};

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationIconInfo& icon_info);

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2);

bool operator==(const WebApplicationIconInfo& icon_info1,
                const WebApplicationIconInfo& icon_info2);

bool operator==(const WebApplicationShortcutsMenuItemInfo::Icon& icon1,
                const WebApplicationShortcutsMenuItemInfo::Icon& icon2);

bool operator==(const WebApplicationShortcutsMenuItemInfo& shortcut_info1,
                const WebApplicationShortcutsMenuItemInfo& shortcut_info2);

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationShortcutsMenuItemInfo& info);

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APPLICATION_INFO_H_
