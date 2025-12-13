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
#include <type_traits>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace base {
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace web_app {

class WebAppInstallManager;
class WebAppProvider;
class ApplyPendingManifestUpdateCommand;
class ManifestSilentUpdateCommand;

using HomeTabIconBitmaps = std::vector<SkBitmap>;
using SquareSizeDip = int;

// Returning metadata about icon bitmaps that are read from the disk.
struct IconMetadataFromDisk {
  IconMetadataFromDisk();
  ~IconMetadataFromDisk();
  IconMetadataFromDisk(IconMetadataFromDisk&& icon_metadata);
  IconMetadataFromDisk& operator=(IconMetadataFromDisk&& icon_metadata);

  SizeToBitmap icons_map;
  IconPurpose purpose = IconPurpose::ANY;
};

// Returning icon metadata about icons that will be shown on the app identity
// update dialog. The `to_icon` can be not populated if there are no pending
// trusted icons.
struct IconMetadataForUpdate {
  IconMetadataForUpdate();
  ~IconMetadataForUpdate();
  IconMetadataForUpdate(IconMetadataForUpdate&& icon_metadata);
  IconMetadataForUpdate& operator=(IconMetadataForUpdate&& icon_metadata);

  SkBitmap from_icon;
  std::optional<SkBitmap> to_icon;
  IconPurpose from_icon_purpose = IconPurpose::ANY;
  std::optional<IconPurpose> to_icon_purpose;
};

using ReadIconMetadataCallback =
    base::OnceCallback<void(IconMetadataFromDisk icon_bitmap_metadata)>;

using ReadIconMetadataForUpdateCallback =
    base::OnceCallback<void(IconMetadataForUpdate icon_bitmap_metadata)>;

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

  // Utility functions that wrap a base::OnceCallback that only reads size to
  // bitmap information into a ReadIconMetadataCallback. Useful to call into
  // ReadTrustedIconsWithFallbackToManifestIcons() if the purpose information is
  // not required for the use-case.
  static ReadIconMetadataCallback BitmapsFromIconMetadataExtractor(
      base::OnceCallback<void(std::map<int, SkBitmap>)> icon_metadata_callback);

  // Writes all data (icons) for an app.
  void WriteData(webapps::AppId app_id,
                 IconBitmaps icon_bitmaps,
                 IconBitmaps trusted_icon_bitmaps,
                 ShortcutsMenuIconBitmaps shortcuts_menu_icons,
                 IconsMap other_icons_map,
                 WriteDataCallback callback);

  // Writes pending icon bitmaps for an app.
  void WritePendingIconData(webapps::AppId app_id,
                            IconBitmaps pending_trusted_icon_bitmaps,
                            IconBitmaps pending_manifest_icon_bitmaps,
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

  // Returns false if any icon in |icon_sizes_in_px| is missing from stored
  // trusted icons for a given app and |purpose|.
  bool HasTrustedIcons(const webapps::AppId& app_id,
                       IconPurpose purpose,
                       const SortedSizesPx& icon_sizes) const;

  struct IconSizeAndPurpose {
    SquareSizePx size_px = 0;
    IconPurpose purpose = IconPurpose::ANY;
    bool is_trusted = false;
  };

  // For each of |purposes|, in the given order, looks for an icon with size at
  // least |min_icon_size|. Returns information on the first icon found.
  std::optional<IconSizeAndPurpose> FindIconMatchBigger(
      const webapps::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size,
      bool skip_trusted_icons_for_favicons = false) const;
  // Returns whether there is a downloaded icon of at least |min_size| for any
  // of the given |purposes|.
  bool HasSmallestIcon(const webapps::AppId& app_id,
                       const std::vector<IconPurpose>& purposes,
                       SquareSizePx min_size) const;

  // Reads the bitmaps for the trusted icon for an app of sizes specified in
  // `icon_sizes`. Returns empty map in `callback` if an IO error happens.
  // The `purpose_for_fallback` information is used as a fallback to read the
  // icon bitmaps obtained from the manifest, mimicking the behavior of
  // ReadUntrustedIcons().
  // By default the callback returns a IconPurpose in it as well. To only get
  // the map of icon size to bitmaps, pass the callback via
  // BitmapsFromIconMetadataExtractor().
  void ReadTrustedIconsWithFallbackToManifestIcons(
      const webapps::AppId& app_id,
      const SortedSizesPx& icon_sizes,
      IconPurpose purpose_for_fallback,
      ReadIconMetadataCallback callback);

  // Returns 2 icons, one from the pending trusted icons folder of `size` and
  // `purpose_for_pending_info` if that is set, and the other from the trusted
  // icons folder of the app of the same `size`. The purpose for the latter icon
  // is determined from the cached icon sizes in the app for correctness.
  void ReadIconsForPendingUpdate(
      const webapps::AppId& app_id,
      SquareSizePx size,
      std::optional<IconPurpose> purpose_for_pending_info,
      ReadIconMetadataForUpdateCallback callback);

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

  // Encapsulate the type of bitmaps being returned by ReadAllIcons().
  struct WebAppBitmaps {
    WebAppBitmaps(IconBitmaps manifest_icons, IconBitmaps trusted_icons)
        : manifest_icons(std::move(manifest_icons)),
          trusted_icons(std::move(trusted_icons)) {}
    WebAppBitmaps() = default;
    ~WebAppBitmaps() = default;

    IconBitmaps manifest_icons;
    IconBitmaps trusted_icons;
  };
  using ReadIconBitmapsCallback =
      base::OnceCallback<void(WebAppBitmaps disk_bitmap)>;
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

  using GetIconsSizeCallback = base::OnceCallback<void(uint64_t)>;
  void GetIconsSizeForApp(const webapps::AppId& app_id,
                          GetIconsSizeCallback callback) const;

  using OverwriteAppIconsFromPendingIconsCallback =
      base::OnceCallback<void(bool success)>;
  void OverwriteAppIconsFromPendingIcons(
      const webapps::AppId& app_id,
      base::PassKey<ApplyPendingManifestUpdateCommand>,
      OverwriteAppIconsFromPendingIconsCallback callback);

  class DeletePendingPassKey {
    friend class ApplyPendingManifestUpdateCommand;
    friend class ManifestSilentUpdateCommand;
    DeletePendingPassKey() = default;
  };
  using DeletePendingIconDataCallback = base::OnceCallback<void(bool success)>;
  void DeletePendingIconData(const webapps::AppId& app_id,
                             DeletePendingPassKey,
                             DeletePendingIconDataCallback callback);

  // Returns a square icon of gfx::kFaviconSize px, or an empty bitmap if not
  // found.
  SkBitmap GetFavicon(const webapps::AppId& app_id) const;

  gfx::ImageSkia GetFaviconImageSkia(const webapps::AppId& app_id) const;
  gfx::ImageSkia GetMonochromeFavicon(const webapps::AppId& app_id) const;

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  using ReadIconsCallback = base::OnceCallback<void(SizeToBitmap icon_bitmaps)>;

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
  // TODO(crbug.com/427566193): Rename here and callsites that this might be
  // untrusted.
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

  // Returns the correct icon file path that exists on the disk for testing.
  // Falls back to using manifest icons if trusted icons are not found.
  base::FilePath GetIconFilePathForTesting(const webapps::AppId& app_id,
                                           IconPurpose purpose,
                                           SquareSizePx size);

  // Returns the pending trusted icon file path that exists on the disk for
  // testing.
  base::FilePath GetAppPendingTrustedIconDirForTesting(
      const webapps::AppId& app_id);

  // Returns the pending trusted icon file path that exists on the disk for
  // testing.
  base::FilePath GetAppPendingManifestIconDirForTesting(
      const webapps::AppId& app_id);

  // Collects icon read/write errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  const std::vector<std::string>* error_log() const { return error_log_.get(); }
  std::vector<std::string>* error_log() { return error_log_.get(); }

 private:
  // Since ReadUntrustedIcons() is only used for testing, have the test class be
  // friended here.
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest, WriteAndReadIcons_AnyOnly);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest,
                           WriteAndReadIcons_MaskableOnly);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest,
                           WriteAndReadIcons_MonochromeOnly);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest,
                           WriteAndReadIcons_AnyAndMaskable);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest,
                           WriteAndReadIcons_AnyAndMonochrome);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest, ReadIconsFailed);
  FRIEND_TEST_ALL_PREFIXES(WebAppIconManagerTest, FindExact);

  base::WeakPtr<const WebAppIconManager> GetWeakPtr() const;
  base::WeakPtr<WebAppIconManager> GetWeakPtr();

  // Reads specified icon bitmaps for an app and |purpose|. These icons are
  // downloaded directly from the manifest and are not always surfaced to the
  // end user, which is why they are untrusted. Returns empty map in |callback|
  // if IO error.
  void ReadUntrustedIcons(const webapps::AppId& app_id,
                          IconPurpose purpose,
                          const SortedSizesPx& icon_sizes,
                          ReadIconMetadataCallback callback);

  std::optional<IconSizeAndPurpose> FindIconMatchSmaller(
      const webapps::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx max_size,
      bool skip_trusted_icons_for_favicons = false) const;

  void OnReadFavicons(ReadImageSkiaCallback callback,
                      IconMetadataFromDisk icon_metadata);

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
