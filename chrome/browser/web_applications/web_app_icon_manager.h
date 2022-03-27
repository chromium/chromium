// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace web_app {

class FileUtilsWrapper;

using SquareSizeDip = int;

// Exclusively used from the UI thread.
class WebAppIconManager : public WebAppInstallManagerObserver {
 public:
  using FaviconReadCallback =
      base::RepeatingCallback<void(const AppId& app_id)>;
  using ReadImageSkiaCallback =
      base::OnceCallback<void(gfx::ImageSkia image_skia)>;

  WebAppIconManager(Profile* profile, scoped_refptr<FileUtilsWrapper> utils);
  WebAppIconManager(const WebAppIconManager&) = delete;
  WebAppIconManager& operator=(const WebAppIconManager&) = delete;
  ~WebAppIconManager() override;

  void SetSubsystems(WebAppRegistrar* registrar,
                     WebAppInstallManager* install_manager);

  using WriteDataCallback = base::OnceCallback<void(bool success)>;

  // Writes all data (icons) for an app.
  void WriteData(AppId app_id,
                 IconBitmaps icon_bitmaps,
                 ShortcutsMenuIconBitmaps shortcuts_menu_icons,
                 IconsMap other_icons_map,
                 WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  void Start();
  void Shutdown();

  // Returns false if any icon in |icon_sizes_in_px| is missing from downloaded
  // icons for a given app and |purpose|.
  bool HasIcons(const AppId& app_id,
                IconPurpose purpose,
                const SortedSizesPx& icon_sizes) const;
  struct IconSizeAndPurpose {
    SquareSizePx size_px = 0;
    IconPurpose purpose = IconPurpose::ANY;
  };
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns information on the first icon found.
  absl::optional<IconSizeAndPurpose> FindIconMatchBigger(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size) const;
  // Returns whether there is a downloaded icon of at least |min_size| for any
  // of the given |purposes|.
  bool HasSmallestIcon(const AppId& app_id,
                       const std::vector<IconPurpose>& purposes,
                       SquareSizePx min_size) const;

  using ReadIconsCallback =
      base::OnceCallback<void(std::map<SquareSizePx, SkBitmap> icon_bitmaps)>;
  // Reads specified icon bitmaps for an app and |purpose|. Returns empty map in
  // |callback| if IO error.
  void ReadIcons(const AppId& app_id,
                 IconPurpose purpose,
                 const SortedSizesPx& icon_sizes,
                 ReadIconsCallback callback) const;

  // TODO (crbug.com/1102701): Callback with const ref instead of value.
  using ReadIconBitmapsCallback =
      base::OnceCallback<void(IconBitmaps icon_bitmaps)>;
  // Reads all icon bitmaps for an app. Returns empty |icon_bitmaps| in
  // |callback| if IO error.
  void ReadAllIcons(const AppId& app_id,
                    ReadIconBitmapsCallback callback) const;

  using ReadShortcutsMenuIconsCallback = base::OnceCallback<void(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps)>;
  // Reads bitmaps for all shortcuts menu icons for an app. Returns a vector of
  // map<SquareSizePx, SkBitmap>. The index of a map in the vector is the same
  // as that of its corresponding shortcut in the manifest's shortcuts vector.
  // Returns empty vector in |callback| if we hit any error.
  void ReadAllShortcutsMenuIcons(const AppId& app_id,
                                 ReadShortcutsMenuIconsCallback callback) const;

  using ReadIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, SkBitmap)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, as a bitmap. Returns
  // an empty SkBitmap in |callback| if IO error.
  void ReadSmallestIcon(const AppId& app_id,
                        const std::vector<IconPurpose>& purposes,
                        SquareSizePx min_size_in_px,
                        ReadIconWithPurposeCallback callback) const;

  using ReadCompressedIconWithPurposeCallback =
      base::OnceCallback<void(IconPurpose, std::vector<uint8_t> data)>;
  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns the first icon found, compressed as PNG.
  // Returns empty |data| in |callback| if IO error.
  void ReadSmallestCompressedIcon(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size_in_px,
      ReadCompressedIconWithPurposeCallback callback) const;

  using ReadIconCallback = base::OnceCallback<void(SkBitmap)>;
  // Convenience method for |ReadSmallestIcon| with IconPurpose::ANY only.
  void ReadSmallestIconAny(const AppId& app_id,
                           SquareSizePx min_icon_size,
                           ReadIconCallback callback) const;

  using ReadCompressedIconCallback =
      base::OnceCallback<void(std::vector<uint8_t> data)>;
  // Convenience method for |ReadSmallestCompressedIcon| with IconPurpose::ANY
  // only.
  void ReadSmallestCompressedIconAny(const AppId& app_id,
                                     SquareSizePx min_icon_size,
                                     ReadCompressedIconCallback callback) const;

  // Returns a square icon of gfx::kFaviconSize px, or an empty bitmap if not
  // found.
  SkBitmap GetFavicon(const AppId& app_id) const;

  gfx::ImageSkia GetFaviconImageSkia(const AppId& app_id) const;
  gfx::ImageSkia GetMonochromeFavicon(const AppId& app_id) const;

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  // Calls back with an icon of the |desired_icon_size| and |purpose|, resizing
  // an icon of a different size if necessary. If no icons were available, calls
  // back with an empty map. Prefers resizing a large icon smaller over resizing
  // a small icon larger.
  void ReadIconAndResize(const AppId& app_id,
                         IconPurpose purpose,
                         SquareSizePx desired_icon_size,
                         ReadIconsCallback callback) const;

  // Reads multiple densities of the icon for each supported UI scale factor.
  // See ui/base/layout.h. Returns null image in |callback| if no icons found
  // for all supported UI scale factors (matches only bigger icons, no
  // upscaling).
  void ReadUiScaleFactorsIcons(const AppId& app_id,
                               IconPurpose purpose,
                               SquareSizeDip size_in_dip,
                               ReadImageSkiaCallback callback);

  void SetFaviconReadCallbackForTesting(FaviconReadCallback callback);
  void SetFaviconMonochromeReadCallbackForTesting(FaviconReadCallback callback);

  // Collects icon read/write errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  const std::vector<std::string>* error_log() const { return error_log_.get(); }
  std::vector<std::string>* error_log() { return error_log_.get(); }

 private:
  static void WrapReadIconWithPurposeCallback(
      ReadIconWithPurposeCallback callback,
      IconPurpose purpose,
      SkBitmap bitmap);

  absl::optional<IconSizeAndPurpose> FindIconMatchSmaller(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx max_size) const;

  void OnReadUiScaleFactorsIcons(SquareSizeDip size_in_dip,
                                 ReadImageSkiaCallback callback,
                                 std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  void ReadFavicon(const AppId& app_id);
  void OnReadFavicon(const AppId& app_id, gfx::ImageSkia image_skia);

  void ReadMonochromeFavicon(const AppId& app_id);
  void OnReadMonochromeFavicon(const AppId& app_id,
                               gfx::ImageSkia manifest_monochrome_image);
  void OnMonochromeIconConverted(const AppId& app_id,
                                 gfx::ImageSkia converted_image);

  raw_ptr<WebAppRegistrar> registrar_;
  raw_ptr<WebAppInstallManager> install_manager_;
  base::FilePath web_apps_directory_;
  scoped_refptr<FileUtilsWrapper> utils_;
  scoped_refptr<base::SequencedTaskRunner> icon_task_runner_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // We cache different densities for high-DPI displays per each app.
  std::map<AppId, gfx::ImageSkia> favicon_cache_;
  std::map<AppId, gfx::ImageSkia> favicon_monochrome_cache_;

  FaviconReadCallback favicon_read_callback_;
  FaviconReadCallback favicon_monochrome_read_callback_;

  std::unique_ptr<std::vector<std::string>> error_log_;

  base::WeakPtrFactory<WebAppIconManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
