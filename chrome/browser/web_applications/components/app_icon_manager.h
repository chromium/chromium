// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_

#include <cstdint>
#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

// Exclusively used from the UI thread.
class AppIconManager {
 public:
  AppIconManager() = default;
  AppIconManager(const AppIconManager&) = delete;
  AppIconManager& operator=(const AppIconManager&) = delete;
  virtual ~AppIconManager() = default;

  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  // Returns false if any icon in |icon_sizes_in_px| is missing from downloaded
  // icons for a given app and |purpose|.
  virtual bool HasIcons(const AppId& app_id,
                        IconPurpose purpose,
                        const SortedSizesPx& icon_sizes_in_px) const = 0;
  struct IconSizeAndPurpose {
    SquareSizePx size_px = 0;
    IconPurpose purpose = IconPurpose::ANY;
  };
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns information on the first icon found.
  virtual base::Optional<IconSizeAndPurpose> FindIconMatchBigger(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size) const = 0;
  // Returns whether there is a downloaded icon of at least |min_size| for any
  // of the given |purposes|.
  virtual bool HasSmallestIcon(const AppId& app_id,
                               const std::vector<IconPurpose>& purposes,
                               SquareSizePx min_size) const = 0;

  using ReadIconsCallback =
      base::OnceCallback<void(std::map<SquareSizePx, SkBitmap> icon_bitmaps)>;
  // Reads specified icon bitmaps for an app and |purpose|. Returns empty map in
  // |callback| if IO error.
  virtual void ReadIcons(const AppId& app_id,
                         IconPurpose purpose,
                         const SortedSizesPx& icon_sizes,
                         ReadIconsCallback callback) const = 0;

  using ReadShortcutsMenuIconsCallback = base::OnceCallback<void(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps)>;

  // Reads bitmaps for all shortcuts menu icons for an app. Returns a vector of
  // map<SquareSizePx, SkBitmap>. The index of a map in the vector is the same
  // as that of its corresponding shortcut in the manifest's shortcuts vector.
  // Returns empty vector in |callback| if we hit any error.
  virtual void ReadAllShortcutsMenuIcons(
      const AppId& app_id,
      ReadShortcutsMenuIconsCallback callback) const = 0;

  // TODO (crbug.com/1102701): Callback with const ref instead of value.
  using ReadIconBitmapsCallback =
      base::OnceCallback<void(IconBitmaps icon_bitmaps)>;
  // Reads all icon bitmaps for an app. Returns empty |icon_bitmaps| in
  // |callback| if IO error.
  virtual void ReadAllIcons(const AppId& app_id,
                            ReadIconBitmapsCallback callback) const = 0;

  using ReadIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, const SkBitmap&)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, as a bitmap. Returns
  // an empty SkBitmap in |callback| if IO error.
  virtual void ReadSmallestIcon(const AppId& app_id,
                                const std::vector<IconPurpose>& purposes,
                                SquareSizePx min_icon_size,
                                ReadIconWithPurposeCallback callback) const = 0;

  using ReadIconCallback = base::OnceCallback<void(const SkBitmap&)>;
  // Convenience method for |ReadSmallestIcon| with IconPurpose::ANY only.
  void ReadSmallestIconAny(const AppId& app_id,
                           SquareSizePx min_icon_size,
                           ReadIconCallback callback) const;

  using ReadCompressedIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, std::vector<uint8_t> data)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, compressed as PNG.
  // Returns empty |data| in |callback| if IO error.
  virtual void ReadSmallestCompressedIcon(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_icon_size,
      ReadCompressedIconWithPurposeCallback callback) const = 0;

  using ReadCompressedIconCallback =
      base::OnceCallback<void(std::vector<uint8_t> data)>;
  // Convenience method for |ReadSmallestCompressedIcon| with IconPurpose::ANY
  // only.
  void ReadSmallestCompressedIconAny(const AppId& app_id,
                                     SquareSizePx min_icon_size,
                                     ReadCompressedIconCallback callback) const;

  // Returns a square icon of gfx::kFaviconSize px, or an empty bitmap if not
  // found.
  virtual SkBitmap GetFavicon(const AppId& app_id) const = 0;

 protected:
  static void WrapReadIconWithPurposeCallback(
      ReadIconWithPurposeCallback callback,
      IconPurpose purpose,
      const SkBitmap& bitmap);

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
