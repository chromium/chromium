// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "web_app_install_info.h"

class Profile;

namespace base {
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace web_app {

class WebAppInstallManager;
class WebAppProvider;

using HomeTabIconBitmaps = std::vector<SkBitmap>;
using SquareSizeDip = int;

// Exclusively used from the UI thread.
class WebAppIconManager : public WebAppInstallManagerObserver {
 public:
  using FaviconReadCallback =
      base::RepeatingCallback<void(const webapps::AppId& app_id)>;
  using ReadImageSkiaCallback =
      base::OnceCallback<void(gfx::ImageSkia image_skia)>;

  explicit WebAppIconManager(Profile* profile);
  WebAppIconManager(const WebAppIconManager&) = delete;
  WebAppIconManager& operator=(const WebAppIconManager&) = delete;
  ~WebAppIconManager() override;

  using WriteDataCallback = base::OnceCallback<void(bool success)>;

  // Writes all data (icons) for an app.
  void WriteData(webapps::AppId app_id,
                 IconBitmaps icon_bitmaps,
                 ShortcutsMenuIconBitmaps shortcuts_menu_icons,
                 IconsMap other_icons_map,
                 WriteDataCallback callback);
  void DeleteData(webapps::AppId app_id, WriteDataCallback callback);

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  // Returns false if any icon in |icon_sizes_in_px| is missing from downloaded
  // icons for a given app and |purpose|.
  bool HasIcons(const webapps::AppId& app_id,
                IconPurpose purpose,
                const SortedSizesPx& icon_sizes) const;
  struct IconSizeAndPurpose {
    SquareSizePx size_px = 0;
    IconPurpose purpose = IconPurpose::ANY;
  };

  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns information on the first icon found.
  std::optional<IconSizeAndPurpose> FindIconMatchBigger(
      const webapps::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size) const;
  // Returns whether there is a downloaded icon of at least |min_size| for any
  // of the given |purposes|.
  bool HasSmallestIcon(const webapps::AppId& app_id,
                       const std::vector<IconPurpose>& purposes,
                       SquareSizePx min_size) const;

  using ReadIconsCallback =
      base::OnceCallback<void(std::map<SquareSizePx, SkBitmap> icon_bitmaps)>;
  // Reads specified icon bitmaps for an app and |purpose|. Returns empty map in
  // |callback| if IO error.
  void ReadIcons(const webapps::AppId& app_id,
                 IconPurpose purpose,
                 const SortedSizesPx& icon_sizes,
                 ReadIconsCallback callback);

  // Mimics WebAppShortcutsMenuItemInfo but stores timestamps instead of icons
  // for os integration.
  using ShortcutMenuIconTimes =
      base::flat_map<IconPurpose, base::flat_map<SquareSizePx, base::Time>>;
  using ShortcutIconDataVector = std::vector<ShortcutMenuIconTimes>;
  using ShortcutIconDataCallback =
      base::OnceCallback<void(ShortcutIconDataVector)>;

  void ReadAllShortcutMenuIconsWithTimestamp(const webapps::AppId& app_id,
                                             ShortcutIconDataCallback callback);

  using ReadIconsUpdateTimeCallback = base::OnceCallback<void(
      base::flat_map<SquareSizePx, base::Time> time_map)>;
  // Reads all the last updated time for all icons in the app. Returns empty map
  // in |callback| if IO error.
  void ReadIconsLastUpdateTime(const webapps::AppId& app_id,
                               ReadIconsUpdateTimeCallback callback);

  // TODO (crbug.com/1102701): Callback with const ref instead of value.
  using ReadIconBitmapsCallback =
      base::OnceCallback<void(IconBitmaps icon_bitmaps)>;
  // Reads all icon bitmaps for an app. Returns empty |icon_bitmaps| in
  // |callback| if IO error.
  void ReadAllIcons(const webapps::AppId& app_id,
                    ReadIconBitmapsCallback callback);

  using ReadShortcutsMenuIconsCallback = base::OnceCallback<void(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps)>;
  // Reads bitmaps for all shortcuts menu icons for an app. Returns a vector of
  // map<SquareSizePx, SkBitmap>. The index of a map in the vector is the same
  // as that of its corresponding shortcut in the manifest's shortcuts vector.
  // Returns empty vector in |callback| if we hit any error.
  void ReadAllShortcutsMenuIcons(const webapps::AppId& app_id,
                                 ReadShortcutsMenuIconsCallback callback);

  using ReadHomeTabIconsCallback =
      base::OnceCallback<void(SkBitmap home_tab_icon_bitmap)>;

  // Reads bitmap for the home tab icon. Returns a SkBitmap
  // in |callback| if the icon exists. Otherwise, if it doesn't
  // exist, the SkBitmap is empty.
  void ReadBestHomeTabIcon(
      const webapps::AppId& app_id,
      const std::vector<blink::Manifest::ImageResource>& icons,
      const SquareSizePx min_home_tab_icon_size_px,
      ReadHomeTabIconsCallback callback);

  using ReadIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, SkBitmap)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, as a bitmap. Returns
  // an empty SkBitmap in |callback| if IO error.
  void ReadSmallestIcon(const webapps::AppId& app_id,
                        const std::vector<IconPurpose>& purposes,
                        SquareSizePx min_size_in_px,
                        ReadIconWithPurposeCallback callback);

  using ReadCompressedIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, std::vector<uint8_t> data)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, compressed as PNG.
  // Returns empty |data| in |callback| if IO error.
  void ReadSmallestCompressedIcon(
      const webapps::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size_in_px,
      ReadCompressedIconWithPurposeCallback callback);

  using ReadCompressedIconsSizeCallback =
      base::OnceCallback<void(const webapps::AppId& app_id, uint64_t size)>;

  using GetIconsSizeCallback = base::OnceCallback<void(uint64_t)>;
  void GetIconsSizeForApp(const webapps::AppId& app_id,
                          GetIconsSizeCallback callback) const;

  // Returns a square icon of gfx::kFaviconSize px, or an empty bitmap if not
  // found.
  SkBitmap GetFavicon(const webapps::AppId& app_id) const;

  gfx::ImageSkia GetFaviconImageSkia(const webapps::AppId& app_id) const;
  gfx::ImageSkia GetMonochromeFavicon(const webapps::AppId& app_id) const;

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  // Calls back with an icon of the |desired_icon_size| and |purpose|, resizing
  // an icon of a different size if necessary. If no icons were available, calls
  // back with an empty map. Prefers resizing a large icon smaller over resizing
  // a small icon larger.
  void ReadIconAndResize(const webapps::AppId& app_id,
                         IconPurpose purpose,
                         SquareSizePx desired_icon_size,
                         ReadIconsCallback callback);

  // Reads multiple densities of the icon for each supported UI scale factor.
  // See ui/base/resource/resource_scale_factor.h. Returns null image in
  // `callback` if no icons found for all supported UI scale factors (matches
  // only bigger icons, no upscaling).
  void ReadFavicons(const webapps::AppId& app_id,
                    IconPurpose purpose,
                    ReadImageSkiaCallback callback);

  struct IconFilesCheck {
    size_t empty = 0;
    size_t missing = 0;
  };
  void CheckForEmptyOrMissingIconFiles(
      const webapps::AppId& app_id,
      base::OnceCallback<void(IconFilesCheck)> callback) const;

  void SetFaviconReadCallbackForTesting(FaviconReadCallback callback);
  void SetFaviconMonochromeReadCallbackForTesting(FaviconReadCallback callback);

  base::FilePath GetIconFilePathForTesting(const webapps::AppId& app_id,
                                           IconPurpose purpose,
                                           SquareSizePx size);

  // Collects icon read/write errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  const std::vector<std::string>* error_log() const { return error_log_.get(); }
  std::vector<std::string>* error_log() { return error_log_.get(); }

 private:
  base::WeakPtr<const WebAppIconManager> GetWeakPtr() const;
  base::WeakPtr<WebAppIconManager> GetWeakPtr();

  std::optional<IconSizeAndPurpose> FindIconMatchSmaller(
      const webapps::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx max_size) const;

  void OnReadFavicons(ReadImageSkiaCallback callback,
                      std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  void ReadFavicon(const webapps::AppId& app_id);
  void OnReadFavicon(const webapps::AppId& app_id, gfx::ImageSkia image_skia);

  void ReadMonochromeFavicon(const webapps::AppId& app_id);
  void OnReadMonochromeFavicon(const webapps::AppId& app_id,
                               gfx::ImageSkia manifest_monochrome_image);
  void OnMonochromeIconConverted(const webapps::AppId& app_id,
                                 gfx::ImageSkia converted_image);

  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::FilePath web_apps_directory_;
  scoped_refptr<base::SequencedTaskRunner> icon_task_runner_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // We cache different densities for high-DPI displays per each app.
  std::map<webapps::AppId, gfx::ImageSkia> favicon_cache_;
  std::map<webapps::AppId, gfx::ImageSkia> favicon_monochrome_cache_;

  FaviconReadCallback favicon_read_callback_;
  FaviconReadCallback favicon_monochrome_read_callback_;

  std::unique_ptr<std::vector<std::string>> error_log_;

  base::WeakPtrFactory<WebAppIconManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
