// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/extend.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/identity.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using ReadCompressedIconCallback =
    base::OnceCallback<void(std::vector<uint8_t> data)>;

using ReadIconCallback = base::OnceCallback<void(SkBitmap)>;

// This utility struct is to carry error logs between threads via return values.
// If we weren't generating multithreaded errors we would just append the errors
// to WebAppIconManager::error_log() directly.
template <typename T>
struct TypedResult {
  T value = T();
  std::vector<std::string> error_log;

  bool HasErrors() const { return !error_log.empty(); }
};

std::string CreateError(std::initializer_list<base::StringPiece> parts) {
  std::string error = base::StrCat(parts);
  LOG(ERROR) << error;
  return error;
}

// This is not a method on WebAppIconManager to avoid having to expose
// TypedResult<T> beyond this cc file.
template <typename T>
void LogErrorsCallCallback(base::WeakPtr<WebAppIconManager> manager,
                           base::OnceCallback<void(T)> callback,
                           TypedResult<T> result) {
  if (!manager)
    return;
  std::vector<std::string>* error_log = manager->error_log();
  if (error_log) {
    base::Extend(*error_log, std::move(result.error_log));
  }

  std::move(callback).Run(std::move(result.value));
}

struct IconId {
  IconId(AppId app_id, IconPurpose purpose, SquareSizePx size)
      : app_id(std::move(app_id)), purpose(purpose), size(size) {}
  ~IconId() = default;

  AppId app_id;
  IconPurpose purpose;
  SquareSizePx size;
};

struct IconSrcAndSize {
  GURL src = GURL();
  SquareSizePx size_px = 0;
};

// This is a private implementation detail of WebAppIconManager, where and how
// to store icon files.
// `app_manifest_resources_directory` is the path to the app-specific
// subdirectory of the profile's manifest resources directory.
base::FilePath GetProductIconsDirectory(
    const base::FilePath& app_manifest_resources_directory,
    IconPurpose purpose) {
  constexpr base::FilePath::CharType kIconsAnyDirectoryName[] =
      FILE_PATH_LITERAL("Icons");
  constexpr base::FilePath::CharType kIconsMonochromeDirectoryName[] =
      FILE_PATH_LITERAL("Icons Monochrome");
  constexpr base::FilePath::CharType kIconsMaskableDirectoryName[] =
      FILE_PATH_LITERAL("Icons Maskable");
  switch (purpose) {
    case IconPurpose::ANY:
      return app_manifest_resources_directory.Append(kIconsAnyDirectoryName);
    case IconPurpose::MONOCHROME:
      return app_manifest_resources_directory.Append(
          kIconsMonochromeDirectoryName);
    case IconPurpose::MASKABLE:
      return app_manifest_resources_directory.Append(
          kIconsMaskableDirectoryName);
  }
}

// This is a private implementation detail of WebAppIconManager, where and how
// to store shortcuts menu icons files.
// All of the other shortcut icon directories appear under the directory for
// |ANY|.
base::FilePath GetAppShortcutsMenuIconsRelativeDirectory(IconPurpose purpose) {
  constexpr base::FilePath::CharType kShortcutsMenuIconsDirectoryName[] =
      FILE_PATH_LITERAL("Shortcuts Menu Icons");

  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMonochromeDirectoryName[] =
          FILE_PATH_LITERAL("Monochrome");
  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMaskableDirectoryName[] =
          FILE_PATH_LITERAL("Maskable");

  base::FilePath shortcuts_icons_directory(kShortcutsMenuIconsDirectoryName);

  switch (purpose) {
    case IconPurpose::ANY:
      return shortcuts_icons_directory;
    case IconPurpose::MONOCHROME:
      return shortcuts_icons_directory.Append(
          kShortcutsMenuIconsMonochromeDirectoryName);
    case IconPurpose::MASKABLE:
      return shortcuts_icons_directory.Append(
          kShortcutsMenuIconsMaskableDirectoryName);
  }
}

base::FilePath GetOtherIconsRelativeDirectory() {
  return base::FilePath(FILE_PATH_LITERAL("Image Cache"));
}

// Returns a string suitable for use as a directory for the given URL. This name
// is a hash of the URL.
std::string GetDirectoryNameForUrl(const GURL& url) {
  return base::NumberToString(base::PersistentHash(url.spec()));
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool DeleteDataBlocking(scoped_refptr<FileUtilsWrapper> utils,
                        const base::FilePath& web_apps_directory,
                        const AppId& app_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);

  return utils->DeleteFileRecursively(app_dir);
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile.
base::FilePath GetIconFileName(const base::FilePath& web_apps_directory,
                               const IconId& icon_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, icon_id.app_id);
  base::FilePath icons_dir = GetProductIconsDirectory(app_dir, icon_id.purpose);

  return icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_id.size));
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile.
base::FilePath GetManifestResourcesShortcutsMenuIconFileName(
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    IconPurpose purpose,
    int index,
    int icon_size_px) {
  const base::FilePath manifest_shortcuts_menu_icons_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id)
          .Append(GetAppShortcutsMenuIconsRelativeDirectory(purpose));
  const base::FilePath manifest_shortcuts_menu_icon_dir =
      manifest_shortcuts_menu_icons_dir.AppendASCII(
          base::NumberToString(index));

  return manifest_shortcuts_menu_icon_dir.AppendASCII(
      base::NumberToString(icon_size_px) + ".png");
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile.
base::FilePath GetManifestResourcesOtherIconsFileName(
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const GURL& url,
    int icon_size_px) {
  return GetManifestResourcesDirectoryForApp(web_apps_directory, app_id)
      .Append(GetOtherIconsRelativeDirectory())
      .AppendASCII(GetDirectoryNameForUrl(url))
      .AppendASCII(base::StringPrintf("%i.png", icon_size_px));
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
TypedResult<SkBitmap> ReadIconBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                       const base::FilePath& web_apps_directory,
                                       const IconId& icon_id) {
  base::FilePath icon_file = GetIconFileName(web_apps_directory, icon_id);
  auto icon_data = base::MakeRefCounted<base::RefCountedString>();
  if (!utils->ReadFileToString(icon_file, &icon_data->data())) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  TypedResult<SkBitmap> result;

  if (!gfx::PNGCodec::Decode(icon_data->front(), icon_data->size(),
                             &result.value)) {
    return {.error_log = {CreateError({"Could not decode icon data for file: ",
                                       icon_file.AsUTF8Unsafe()})}};
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
// Returns null base::Time if any errors occurred.
TypedResult<base::Time> ReadIconTimeBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    base::FilePath icon_file) {
  base::File::Info file_info;
  if (!utils->GetFileInfo(icon_file, &file_info)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  TypedResult<base::Time> access_time;
  access_time.value = base::Time();
  if (!file_info.last_modified.is_null()) {
    access_time.value = file_info.last_modified;
  }
  return access_time;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
TypedResult<SkBitmap> ReadShortcutsMenuIconBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    IconPurpose purpose,
    int index,
    int icon_size_px) {
  base::FilePath icon_file = GetManifestResourcesShortcutsMenuIconFileName(
      web_apps_directory, app_id, purpose, index, icon_size_px);

  std::string icon_data;

  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  TypedResult<SkBitmap> result;

  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(icon_data.c_str()),
          icon_data.size(), &result.value)) {
    return {.error_log = {CreateError({"Could not decode icon data for file: ",
                                       icon_file.AsUTF8Unsafe()})}};
  }

  return result;
}

// Returns empty SkBitmap if any errors occurred.
TypedResult<SkBitmap> ReadHomeTabIconFromFileAndResizeBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const GURL& url,
    int icon_size_px,
    const SquareSizePx target_icon_size_px) {
  base::FilePath icon_file = GetManifestResourcesOtherIconsFileName(
      web_apps_directory, app_id, url, icon_size_px);

  std::string icon_data;
  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  TypedResult<SkBitmap> result;
  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(icon_data.c_str()),
          icon_data.size(), &result.value)) {
    return {.error_log = {CreateError({"Could not decode icon data for file: ",
                                       icon_file.AsUTF8Unsafe()})}};
  }

  if (result.value.width() != target_icon_size_px) {
    result.value = skia::ImageOperations::Resize(
        result.value, skia::ImageOperations::RESIZE_BEST, target_icon_size_px,
        target_icon_size_px);
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty map if any errors occurred.
TypedResult<std::map<SquareSizePx, SkBitmap>> ReadIconAndResizeBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id,
    SquareSizePx target_icon_size_px) {
  TypedResult<std::map<SquareSizePx, SkBitmap>> result;

  TypedResult<SkBitmap> read_result =
      ReadIconBlocking(std::move(utils), web_apps_directory, icon_id);
  if (read_result.HasErrors())
    return {.error_log = std::move(read_result.error_log)};

  SkBitmap source = std::move(read_result.value);
  SkBitmap target;

  if (icon_id.size != target_icon_size_px) {
    target = skia::ImageOperations::Resize(
        source, skia::ImageOperations::RESIZE_BEST, target_icon_size_px,
        target_icon_size_px);
  } else {
    target = std::move(source);
  }

  result.value[target_icon_size_px] = std::move(target);
  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<std::map<SquareSizePx, SkBitmap>> ReadIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    IconPurpose purpose,
    const std::vector<SquareSizePx>& icon_sizes) {
  TypedResult<std::map<SquareSizePx, SkBitmap>> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    IconId icon_id(app_id, purpose, icon_size_px);
    TypedResult<SkBitmap> read_result =
        ReadIconBlocking(utils, web_apps_directory, icon_id);
    base::Extend(result.error_log, std::move(read_result.error_log));
    if (!read_result.value.empty())
      result.value[icon_size_px] = std::move(read_result.value);
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<base::flat_map<SquareSizePx, base::Time>>
ReadIconsLastUpdateTimeBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                const base::FilePath& web_apps_directory,
                                const AppId& app_id,
                                IconPurpose purpose,
                                const std::vector<SquareSizePx>& icon_sizes) {
  TypedResult<base::flat_map<SquareSizePx, base::Time>> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    IconId icon_id(app_id, purpose, icon_size_px);
    base::FilePath icon_file = GetIconFileName(web_apps_directory, icon_id);
    TypedResult<base::Time> read_result =
        ReadIconTimeBlocking(utils, icon_file);
    base::Extend(result.error_log, std::move(read_result.error_log));
    if (!read_result.value.is_null())
      result.value[icon_size_px] = std::move(read_result.value);
  }

  return result;
}

TypedResult<IconBitmaps> ReadAllIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::map<IconPurpose, std::vector<SquareSizePx>>&
        icon_purposes_to_sizes) {
  TypedResult<IconBitmaps> result;

  for (const auto& purpose_sizes : icon_purposes_to_sizes) {
    TypedResult<std::map<SquareSizePx, SkBitmap>> read_result =
        ReadIconsBlocking(utils, web_apps_directory, app_id,
                          purpose_sizes.first, purpose_sizes.second);
    base::Extend(result.error_log, std::move(read_result.error_log));
    result.value.SetBitmapsForPurpose(purpose_sizes.first,
                                      std::move(read_result.value));
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<ShortcutsMenuIconBitmaps> ReadShortcutsMenuIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::vector<IconSizes>& shortcuts_menu_icons_sizes) {
  TypedResult<ShortcutsMenuIconBitmaps> results;
  int curr_index = 0;
  for (const auto& icon_sizes : shortcuts_menu_icons_sizes) {
    IconBitmaps result;

    for (IconPurpose purpose : kIconPurposes) {
      std::map<SquareSizePx, SkBitmap> bitmaps;

      for (SquareSizePx icon_size_px : icon_sizes.GetSizesForPurpose(purpose)) {
        TypedResult<SkBitmap> read_result =
            ReadShortcutsMenuIconBlocking(utils, web_apps_directory, app_id,
                                          purpose, curr_index, icon_size_px);
        base::Extend(results.error_log, std::move(read_result.error_log));
        if (!read_result.value.empty())
          bitmaps[icon_size_px] = std::move(read_result.value);
      }

      result.SetBitmapsForPurpose(purpose, std::move(bitmaps));
    }

    ++curr_index;
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.value.push_back(std::move(result));
  }
  CHECK_EQ(shortcuts_menu_icons_sizes.size(), results.value.size());
  return results;
}

// Returns an IconSrcAndSize for the smallest icon that is larger than the
// minimum size specified. If there is no icon larger than the minimum size
// the IconSrcAndSize of the largest icon available is returned. An icon
// prioritises matching purpose before size. The purpose of the returned icon is
// specified by the decreasing order of preference in |purposes|.
absl::optional<IconSrcAndSize> FindBestImageResourceMatch(
    const std::vector<IconPurpose>& purposes,
    const std::vector<blink::Manifest::ImageResource>& icons,
    SquareSizePx min_size) {
  IconSrcAndSize best_match;
  IconSrcAndSize biggest_icon;
  best_match.size_px = INT_MAX;
  biggest_icon.size_px = INT_MIN;

  for (const IconPurpose& purpose : purposes) {
    for (const blink::Manifest::ImageResource& icon : icons) {
      // TODO(crbug.com/1381377): Need to add check if icon has been
      // successfully downloaded.
      if (!base::Contains(icon.purpose, purpose)) {
        continue;
      }
      for (const gfx::Size& icon_size : icon.sizes) {
        if (icon_size.width() > biggest_icon.size_px) {
          biggest_icon.size_px = icon_size.width();
          biggest_icon.src = icon.src;
        }
        if (icon_size.width() < min_size) {
          continue;
        }
        if (icon_size.width() > best_match.size_px) {
          continue;
        }
        best_match.size_px = icon_size.width();
        best_match.src = icon.src;
      }
    }
    if (!best_match.src.is_empty()) {
      return best_match;
    }
    if (!biggest_icon.src.is_empty()) {
      return biggest_icon;
    }
  }

  return absl::nullopt;
}

TypedResult<SkBitmap> ReadHomeTabIconBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::vector<blink::Manifest::ImageResource>& icons,
    const SquareSizePx min_home_tab_icon_size_px) {
  absl::optional<IconSrcAndSize> best_icon = FindBestImageResourceMatch(
      {IconPurpose::ANY}, icons, min_home_tab_icon_size_px);
  TypedResult<SkBitmap> result;

  // Returns empty SkBitmap if |best_icon| not found
  if (!best_icon.has_value()) {
    result.error_log = {CreateError({"No suitable home tab icon found"})};
  } else {
    result = ReadHomeTabIconFromFileAndResizeBlocking(
        utils, web_apps_directory, app_id, best_icon->src, best_icon->size_px,
        min_home_tab_icon_size_px);
  }

  return result;
}

TypedResult<WebAppIconManager::ShortcutIconDataVector>
ReadShortcutMenuIconsWithTimestampBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::vector<IconSizes>& shortcuts_menu_icons_sizes) {
  TypedResult<WebAppIconManager::ShortcutIconDataVector> results;
  int curr_index = 0;
  for (const auto& icon_sizes : shortcuts_menu_icons_sizes) {
    WebAppIconManager::ShortcutMenuIconTimes data;
    for (IconPurpose purpose : kIconPurposes) {
      base::flat_map<SquareSizePx, base::Time> bitmap_with_time;
      for (SquareSizePx icon_size_px : icon_sizes.GetSizesForPurpose(purpose)) {
        base::FilePath file_name =
            GetManifestResourcesShortcutsMenuIconFileName(
                web_apps_directory, app_id, purpose, curr_index, icon_size_px);
        TypedResult<base::Time> read_result =
            ReadIconTimeBlocking(utils, file_name);
        base::Extend(results.error_log, std::move(read_result.error_log));
        if (!read_result.value.is_null()) {
          bitmap_with_time[icon_size_px] = std::move(read_result.value);
        }
      }
      data[purpose] = bitmap_with_time;
    }
    ++curr_index;
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.value.push_back(std::move(data));
  }
  CHECK_EQ(shortcuts_menu_icons_sizes.size(), results.value.size());
  return results;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty vector if any errors occurred.
TypedResult<std::vector<uint8_t>> ReadCompressedIconBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id) {
  base::FilePath icon_file = GetIconFileName(web_apps_directory, icon_id);

  std::string icon_data;

  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  // Copy data: we can't std::move std::string into std::vector.
  return {.value = {icon_data.begin(), icon_data.end()}};
}

WebAppIconManager::IconFilesCheck CheckForEmptyOrMissingIconFilesBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    base::flat_map<IconPurpose, SortedSizesPx> purpose_to_sizes) {
  WebAppIconManager::IconFilesCheck result;
  for (auto it : purpose_to_sizes) {
    const IconPurpose& purpose = it.first;
    const SortedSizesPx& square_sizes = it.second;
    for (SquareSizePx size : square_sizes) {
      base::FilePath icon_path =
          GetIconFileName(web_apps_directory, IconId(app_id, purpose, size));
      base::File::Info file_info;
      if (utils->GetFileInfo(icon_path, &file_info)) {
        if (file_info.size == 0)
          ++result.empty;
      } else {
        ++result.missing;
      }
    }
  }
  return result;
}

gfx::ImageSkia ConvertUiScaleFactorsBitmapsToImageSkia(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps,
    SquareSizeDip size_in_dip) {
  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();
  for (ui::ResourceScaleFactor scale_factor :
       ui::GetSupportedResourceScaleFactors()) {
    float icon_scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    SquareSizePx icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_in_dip, size_in_dip), icon_scale)
            .width();

    while (it != icon_bitmaps.end() && it->first < icon_size_in_px)
      ++it;

    if (it == icon_bitmaps.end() || it->second.empty())
      break;

    SkBitmap bitmap = it->second;

    // Resize |bitmap| to match |icon_scale|.
    if (bitmap.width() != icon_size_in_px) {
      bitmap = skia::ImageOperations::Resize(bitmap,
                                             skia::ImageOperations::RESIZE_BEST,
                                             icon_size_in_px, icon_size_in_px);
    }

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, icon_scale));
  }

  return image_skia;
}

// A utility that manages writing icons to disk for a single app. Should only be
// used on an I/O thread.
class WriteIconsJob {
 public:
  static TypedResult<bool> WriteIconsBlocking(
      scoped_refptr<FileUtilsWrapper> utils,
      base::FilePath&& web_apps_directory,
      AppId&& app_id,
      IconBitmaps&& icon_bitmaps,
      ShortcutsMenuIconBitmaps&& shortcuts_menu_icon_bitmaps,
      IconsMap&& other_icons) {
    WriteIconsJob job(std::move(utils), std::move(web_apps_directory),
                      std::move(app_id), std::move(icon_bitmaps),
                      std::move(shortcuts_menu_icon_bitmaps),
                      std::move(other_icons));
    return job.Execute();
  }

  WriteIconsJob(const WriteIconsJob& other) = delete;
  WriteIconsJob& operator=(const WriteIconsJob& other) = delete;

 private:
  WriteIconsJob(scoped_refptr<FileUtilsWrapper> utils,
                base::FilePath&& web_apps_directory,
                AppId&& app_id,
                IconBitmaps&& icon_bitmaps,
                ShortcutsMenuIconBitmaps&& shortcuts_menu_icon_bitmaps,
                IconsMap&& other_icons)
      : utils_(std::move(utils)),
        web_apps_directory_(std::move(web_apps_directory)),
        app_id_(std::move(app_id)),
        icon_bitmaps_(std::move(icon_bitmaps)),
        shortcuts_menu_icon_bitmaps_(std::move(shortcuts_menu_icon_bitmaps)),
        other_icons_(std::move(other_icons)) {}
  ~WriteIconsJob() = default;

  TypedResult<bool> Execute() {
    // Write product icons directly in the app's directory.
    auto result = AtomicallyWriteIcons(
        base::BindRepeating(&WriteIconsJob::WriteProductIcons,
                            base::Unretained(this)),
        /*subdir_for_icons=*/{});
    if (result.HasErrors())
      return result;

    if (!shortcuts_menu_icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteShortcutsMenuIcons,
                              base::Unretained(this)),
          /*subdir_for_icons=*/GetAppShortcutsMenuIconsRelativeDirectory(
              IconPurpose::ANY));

      if (result.HasErrors())
        return result;
    }

    if (!other_icons_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteOtherIcons,
                              base::Unretained(this)),
          /*subdir_for_icons=*/
          GetOtherIconsRelativeDirectory());
    }

    return result;
  }

  // Manages writing a set of icons to a particular location on disk, making a
  // best-effort to make it all-or-nothing. Returns true if no errors occurred.
  // This is used for several kinds of icon data. The passed callbacks allow for
  // varying the implementation based on data type. `write_icons_callback` is
  // expected to write the icons data under the passed base directory.
  // `subdir_for_icons` is a relative FilePath representing a directory which
  // holds all the data written by `write_icons_callback`. The path is relative
  // to the app's manifest resources directory.
  TypedResult<bool> AtomicallyWriteIcons(
      const base::RepeatingCallback<
          TypedResult<bool>(const base::FilePath& path)>& write_icons_callback,
      const base::FilePath& subdir_for_icons) {
    DCHECK(!subdir_for_icons.IsAbsolute());
    // Create the temp directory under the web apps root.
    // This guarantees it is on the same file system as the WebApp's eventual
    // install target.
    base::FilePath temp_dir = GetWebAppsTempDirectory(web_apps_directory_);
    TypedResult<bool> create_result = CreateDirectory(temp_dir);
    if (create_result.HasErrors())
      return create_result;

    base::ScopedTempDir app_temp_dir;
    if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir)) {
      return {
          .error_log = {CreateError({"Could not create temp directory under: ",
                                     temp_dir.AsUTF8Unsafe()})}};
    }

    base::FilePath manifest_resources_directory =
        GetManifestResourcesDirectory(web_apps_directory_);
    create_result = CreateDirectory(manifest_resources_directory);
    if (create_result.HasErrors())
      return create_result;

    TypedResult<bool> write_result =
        write_icons_callback.Run(app_temp_dir.GetPath());
    if (write_result.HasErrors())
      return write_result;

    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_directory_, app_id_);
    base::FilePath final_icons_dir = app_dir.Append(subdir_for_icons);
    // Create app_dir if it doesn't already exist. We'll need this for
    // WriteShortcutsMenuIconsData unittests.
    if (final_icons_dir != app_dir) {
      create_result = CreateDirectory(app_dir);
      if (create_result.HasErrors())
        return create_result;
    }

    // Delete the destination. Needed for update. Ignore the result.
    utils_->DeleteFileRecursively(final_icons_dir);

    base::FilePath temp_icons_dir =
        app_temp_dir.GetPath().Append(subdir_for_icons);
    // Commit: move whole icons data dir to final destination in one mv
    // operation.
    if (!utils_->Move(temp_icons_dir, final_icons_dir)) {
      return {.error_log = {CreateError(
                  {"Could not move: ", temp_icons_dir.AsUTF8Unsafe(),
                   " to: ", final_icons_dir.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  TypedResult<bool> WriteProductIcons(const base::FilePath& base_dir) {
    for (IconPurpose purpose : kIconPurposes) {
      base::FilePath icons_dir = GetProductIconsDirectory(base_dir, purpose);

      auto create_result = CreateDirectory(icons_dir);
      if (create_result.HasErrors())
        return create_result;

      for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
           icon_bitmaps_.GetBitmapsForPurpose(purpose)) {
        TypedResult<bool> write_result =
            EncodeAndWriteIcon(icons_dir, icon_bitmap.second);
        if (write_result.HasErrors())
          return write_result;
      }
    }

    return {.value = true};
  }

  // Writes shortcuts menu icons files to the Shortcut Icons directory. Creates
  // a new directory per shortcut item using its index in the vector.
  TypedResult<bool> WriteShortcutsMenuIcons(
      const base::FilePath& app_manifest_resources_directory) {
    for (IconPurpose purpose : kIconPurposes) {
      const base::FilePath shortcuts_menu_icons_dir =
          app_manifest_resources_directory.Append(
              GetAppShortcutsMenuIconsRelativeDirectory(purpose));
      auto create_result = CreateDirectory(shortcuts_menu_icons_dir);
      if (create_result.HasErrors())
        return create_result;

      int shortcut_index = -1;
      for (const IconBitmaps& icon_bitmaps : shortcuts_menu_icon_bitmaps_) {
        ++shortcut_index;
        const std::map<SquareSizePx, SkBitmap>& bitmaps =
            icon_bitmaps.GetBitmapsForPurpose(purpose);
        if (bitmaps.empty())
          continue;

        const base::FilePath shortcuts_menu_icon_dir =
            shortcuts_menu_icons_dir.AppendASCII(
                base::NumberToString(shortcut_index));
        create_result = CreateDirectory(shortcuts_menu_icon_dir);
        if (create_result.HasErrors())
          return create_result;

        for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
             bitmaps) {
          TypedResult<bool> write_result =
              EncodeAndWriteIcon(shortcuts_menu_icon_dir, icon_bitmap.second);
          if (write_result.HasErrors())
            return write_result;
        }
      }
    }
    return {.value = true};
  }

  TypedResult<bool> WriteOtherIcons(
      const base::FilePath& app_manifest_resources_directory) {
    const base::FilePath general_icons_dir =
        app_manifest_resources_directory.Append(
            GetOtherIconsRelativeDirectory());
    auto create_result = CreateDirectory(general_icons_dir);
    if (create_result.HasErrors())
      return create_result;

    for (const std::pair<const GURL, std::vector<SkBitmap>>& entry :
         other_icons_) {
      const base::FilePath subdir =
          general_icons_dir.AppendASCII(GetDirectoryNameForUrl(entry.first));
      create_result = CreateDirectory(subdir);
      if (create_result.HasErrors())
        return create_result;

      const std::vector<SkBitmap>& icon_bitmaps = entry.second;
      for (const SkBitmap& icon_bitmap : icon_bitmaps) {
        TypedResult<bool> write_result =
            EncodeAndWriteIcon(subdir, icon_bitmap);
        if (write_result.HasErrors())
          return write_result;
      }
    }
    return {.value = true};
  }

  TypedResult<bool> CreateDirectory(const base::FilePath& path) {
    if (!utils_->CreateDirectory(path)) {
      return {.error_log = {CreateError(
                  {"Could not create directory: ", path.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  // Encodes `bitmap` as a PNG and writes to the given directory.
  TypedResult<bool> EncodeAndWriteIcon(const base::FilePath& icons_dir,
                                       const SkBitmap& bitmap) {
    DCHECK_NE(bitmap.colorType(), kUnknown_SkColorType);
    DCHECK_EQ(bitmap.width(), bitmap.height());
    base::FilePath icon_file =
        icons_dir.AppendASCII(base::StringPrintf("%i.png", bitmap.width()));

    std::vector<unsigned char> image_data;
    const bool discard_transparency = false;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                           &image_data)) {
      return {.error_log = {CreateError({"Could not encode icon data for file ",
                                         icon_file.AsUTF8Unsafe()})}};
    }

    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    int size = base::checked_cast<int>(image_data.size());
    if (utils_->WriteFile(icon_file, image_data_ptr, size) != size) {
      return {.error_log = {CreateError(
                  {"Could not write icon file: ", icon_file.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  scoped_refptr<FileUtilsWrapper> utils_;
  base::FilePath web_apps_directory_;
  AppId app_id_;
  IconBitmaps icon_bitmaps_;
  ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps_;
  SkBitmap home_tab_icon_bitmap_;
  IconsMap other_icons_;
};

}  // namespace

WebAppIconManager::WebAppIconManager(Profile* profile,
                                     scoped_refptr<FileUtilsWrapper> utils)
    : utils_(std::move(utils)),
      icon_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  web_apps_directory_ = GetWebAppsRootDirectory(profile);
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo))
    error_log_ = std::make_unique<std::vector<std::string>>();
}

WebAppIconManager::~WebAppIconManager() = default;

void WebAppIconManager::SetSubsystems(WebAppRegistrar* registrar,
                                      WebAppInstallManager* install_manager) {
  registrar_ = registrar;
  install_manager_ = install_manager;
}

void WebAppIconManager::WriteData(
    AppId app_id,
    IconBitmaps icon_bitmaps,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps,
    IconsMap other_icons_map,
    WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &WriteIconsJob::WriteIconsBlocking, utils_, web_apps_directory_,
          std::move(app_id), std::move(icon_bitmaps),
          std::move(shortcuts_menu_icon_bitmaps), std::move(other_icons_map)),
      base::BindOnce(&LogErrorsCallCallback<bool>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::DeleteData(AppId app_id, WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(DeleteDataBlocking, utils_, web_apps_directory_,
                     std::move(app_id)),
      std::move(callback));
}

void WebAppIconManager::Start() {
  for (const AppId& app_id : registrar_->GetAppIds()) {
    ReadFavicon(app_id);

#if BUILDFLAG(IS_CHROMEOS)
    // Notifications use a monochrome icon.
    ReadMonochromeFavicon(app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  install_manager_observation_.Observe(install_manager_);
}

void WebAppIconManager::Shutdown() {}

bool WebAppIconManager::HasIcons(const AppId& app_id,
                                 IconPurpose purpose,
                                 const SortedSizesPx& icon_sizes) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app)
    return false;

  return base::ranges::includes(web_app->downloaded_icon_sizes(purpose),
                                icon_sizes);
}

absl::optional<WebAppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchBigger(const AppId& app_id,
                                       const std::vector<IconPurpose>& purposes,
                                       SquareSizePx min_size) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  // Must iterate through purposes in order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from smallest to largest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : sizes) {
      if (size >= min_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return absl::nullopt;
}

bool WebAppIconManager::HasSmallestIcon(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size) const {
  return FindIconMatchBigger(app_id, purposes, min_size).has_value();
}

void WebAppIconManager::ReadIcons(const AppId& app_id,
                                  IconPurpose purpose,
                                  const SortedSizesPx& icon_sizes,
                                  ReadIconsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!registrar_->GetAppById(app_id)) {
    std::move(callback).Run(std::map<SquareSizePx, SkBitmap>());
    return;
  }
  DCHECK(HasIcons(app_id, purpose, icon_sizes));

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ReadIconsBlocking, utils_, web_apps_directory_, app_id, purpose,
          std::vector<SquareSizePx>(icon_sizes.begin(), icon_sizes.end())),
      base::BindOnce(&LogErrorsCallCallback<std::map<SquareSizePx, SkBitmap>>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadAllShortcutMenuIconsWithTimestamp(
    const AppId& app_id,
    ShortcutIconDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(ShortcutIconDataVector());
    return;
  }
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadShortcutMenuIconsWithTimestampBlocking, utils_,
                     web_apps_directory_, app_id,
                     web_app->downloaded_shortcuts_menu_icons_sizes()),
      base::BindOnce(&LogErrorsCallCallback<ShortcutIconDataVector>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadIconsLastUpdateTime(
    const AppId& app_id,
    ReadIconsUpdateTimeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(base::flat_map<SquareSizePx, base::Time>());
    return;
  }

  const SortedSizesPx& sizes_px =
      web_app->downloaded_icon_sizes(IconPurpose::ANY);
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ReadIconsLastUpdateTimeBlocking, utils_, web_apps_directory_, app_id,
          IconPurpose::ANY,
          std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end())),
      base::BindOnce(
          &LogErrorsCallCallback<base::flat_map<SquareSizePx, base::Time>>,
          GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadAllIcons(const AppId& app_id,
                                     ReadIconBitmapsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(IconBitmaps());
    return;
  }

  std::map<IconPurpose, std::vector<SquareSizePx>> icon_purposes_to_sizes;

  for (IconPurpose purpose : kIconPurposes) {
    const SortedSizesPx& sizes_px = web_app->downloaded_icon_sizes(purpose);
    icon_purposes_to_sizes[purpose] =
        std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end());
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadAllIconsBlocking, utils_, web_apps_directory_, app_id,
                     std::move(icon_purposes_to_sizes)),
      base::BindOnce(&LogErrorsCallCallback<IconBitmaps>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::ReadAllShortcutsMenuIcons(
    const AppId& app_id,
    ReadShortcutsMenuIconsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(ShortcutsMenuIconBitmaps{});
    return;
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadShortcutsMenuIconsBlocking, utils_,
                     web_apps_directory_, app_id,
                     web_app->downloaded_shortcuts_menu_icons_sizes()),
      base::BindOnce(&LogErrorsCallCallback<ShortcutsMenuIconBitmaps>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadBestHomeTabIcon(
    const AppId& app_id,
    const std::vector<blink::Manifest::ImageResource>& icons,
    const SquareSizePx min_home_tab_icon_size_px,
    ReadHomeTabIconsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(SkBitmap());
    return;
  }
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadHomeTabIconBlocking, utils_, web_apps_directory_,
                     app_id, icons, min_home_tab_icon_size_px),
      base::BindOnce(&LogErrorsCallCallback<SkBitmap>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::ReadSmallestIcon(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadIconWithPurposeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  DCHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadIconCallback wrapped =
      base::BindOnce(std::move(callback), best_icon->purpose);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadIconBlocking, utils_, web_apps_directory_,
                     std::move(icon_id)),
      base::BindOnce(&LogErrorsCallCallback<SkBitmap>, GetWeakPtr(),
                     std::move(wrapped)));
}

void WebAppIconManager::ReadSmallestCompressedIcon(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadCompressedIconWithPurposeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  DCHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadCompressedIconCallback wrapped =
      base::BindOnce(std::move(callback), best_icon->purpose);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadCompressedIconBlocking, utils_, web_apps_directory_,
                     std::move(icon_id)),
      base::BindOnce(&LogErrorsCallCallback<std::vector<uint8_t>>, GetWeakPtr(),
                     std::move(wrapped)));
}

SkBitmap WebAppIconManager::GetFavicon(const AppId& app_id) const {
  auto iter = favicon_cache_.find(app_id);
  if (iter == favicon_cache_.end())
    return SkBitmap();

  const gfx::ImageSkia& image_skia = iter->second;

  // A representation for 1.0 UI scale factor is mandatory. GetRepresentation()
  // should create one.
  return image_skia.GetRepresentation(1.0f).GetBitmap();
}

gfx::ImageSkia WebAppIconManager::GetFaviconImageSkia(
    const AppId& app_id) const {
  auto iter = favicon_cache_.find(app_id);
  return iter != favicon_cache_.end() ? iter->second : gfx::ImageSkia();
}

gfx::ImageSkia WebAppIconManager::GetMonochromeFavicon(
    const AppId& app_id) const {
  auto iter = favicon_monochrome_cache_.find(app_id);
  return iter != favicon_monochrome_cache_.end() ? iter->second
                                                 : gfx::ImageSkia();
}

void WebAppIconManager::OnWebAppInstalled(const AppId& app_id) {
  ReadFavicon(app_id);
#if BUILDFLAG(IS_CHROMEOS)
  // Notifications use a monochrome icon.
  ReadMonochromeFavicon(app_id);
#endif
}

void WebAppIconManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppIconManager::ReadIconAndResize(const AppId& app_id,
                                          IconPurpose purpose,
                                          SquareSizePx desired_icon_size,
                                          ReadIconsCallback callback) {
  absl::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, {purpose}, desired_icon_size);
  if (!best_icon) {
    best_icon = FindIconMatchSmaller(app_id, {purpose}, desired_icon_size);
  }

  if (!best_icon) {
    std::move(callback).Run(std::map<SquareSizePx, SkBitmap>());
    return;
  }

  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadIconAndResizeBlocking, utils_, web_apps_directory_,
                     std::move(icon_id), desired_icon_size),
      base::BindOnce(&LogErrorsCallCallback<std::map<SquareSizePx, SkBitmap>>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadUiScaleFactorsIcons(
    const AppId& app_id,
    IconPurpose purpose,
    SquareSizeDip size_in_dip,
    ReadImageSkiaCallback callback) {
  SortedSizesPx ui_scale_factors_px_sizes;
  for (ui::ResourceScaleFactor scale_factor :
       ui::GetSupportedResourceScaleFactors()) {
    auto size_and_purpose = FindIconMatchBigger(
        app_id, {purpose},
        gfx::ScaleToFlooredSize(
            gfx::Size(size_in_dip, size_in_dip),
            ui::GetScaleForResourceScaleFactor(scale_factor))
            .width());
    if (size_and_purpose.has_value())
      ui_scale_factors_px_sizes.insert(size_and_purpose->size_px);
  }

  if (ui_scale_factors_px_sizes.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  ReadIcons(app_id, purpose, ui_scale_factors_px_sizes,
            base::BindOnce(&WebAppIconManager::OnReadUiScaleFactorsIcons,
                           GetWeakPtr(), size_in_dip, std::move(callback)));
}

void WebAppIconManager::OnReadUiScaleFactorsIcons(
    SquareSizeDip size_in_dip,
    ReadImageSkiaCallback callback,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  std::move(callback).Run(
      ConvertUiScaleFactorsBitmapsToImageSkia(icon_bitmaps, size_in_dip));
}

void WebAppIconManager::CheckForEmptyOrMissingIconFiles(
    const AppId& app_id,
    base::OnceCallback<void(IconFilesCheck)> callback) const {
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run({});
    return;
  }

  base::flat_map<IconPurpose, SortedSizesPx> purpose_to_sizes;
  for (IconPurpose purpose : kIconPurposes)
    purpose_to_sizes[purpose] = web_app->downloaded_icon_sizes(purpose);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(CheckForEmptyOrMissingIconFilesBlocking, utils_,
                     web_apps_directory_, app_id, std::move(purpose_to_sizes)),
      std::move(callback));
}

void WebAppIconManager::SetFaviconReadCallbackForTesting(
    FaviconReadCallback callback) {
  favicon_read_callback_ = std::move(callback);
}

void WebAppIconManager::SetFaviconMonochromeReadCallbackForTesting(
    FaviconReadCallback callback) {
  favicon_monochrome_read_callback_ = std::move(callback);
}

base::FilePath WebAppIconManager::GetIconFilePathForTesting(const AppId& app_id,
                                                            IconPurpose purpose,
                                                            SquareSizePx size) {
  return GetIconFileName(web_apps_directory_, IconId(app_id, purpose, size));
}

base::WeakPtr<const WebAppIconManager> WebAppIconManager::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<WebAppIconManager> WebAppIconManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

absl::optional<WebAppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchSmaller(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx max_size) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  // Must check purposes in the order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from largest to smallest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : base::Reversed(sizes)) {
      if (size <= max_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return absl::nullopt;
}

void WebAppIconManager::ReadFavicon(const AppId& app_id) {
  ReadUiScaleFactorsIcons(
      app_id, IconPurpose::ANY, gfx::kFaviconSize,
      base::BindOnce(&WebAppIconManager::OnReadFavicon, GetWeakPtr(), app_id));
}

void WebAppIconManager::OnReadFavicon(const AppId& app_id,
                                      gfx::ImageSkia image_skia) {
  if (!image_skia.isNull())
    favicon_cache_[app_id] = image_skia;

  if (favicon_read_callback_)
    favicon_read_callback_.Run(app_id);
}

void WebAppIconManager::ReadMonochromeFavicon(const AppId& app_id) {
  ReadUiScaleFactorsIcons(
      app_id, IconPurpose::MONOCHROME, gfx::kFaviconSize,
      base::BindOnce(&WebAppIconManager::OnReadMonochromeFavicon, GetWeakPtr(),
                     app_id));
}

void WebAppIconManager::OnReadMonochromeFavicon(
    const AppId& app_id,
    gfx::ImageSkia manifest_monochrome_image) {
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app)
    return;

  if (manifest_monochrome_image.isNull()) {
    OnMonochromeIconConverted(app_id, manifest_monochrome_image);
    return;
  }

  const SkColor solid_color =
      web_app->theme_color() ? *web_app->theme_color() : SK_ColorDKGRAY;

  manifest_monochrome_image.MakeThreadSafe();

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ConvertImageToSolidFillMonochrome, solid_color,
                     std::move(manifest_monochrome_image)),
      base::BindOnce(&WebAppIconManager::OnMonochromeIconConverted,
                     GetWeakPtr(), app_id));
}

void WebAppIconManager::OnMonochromeIconConverted(
    const AppId& app_id,
    gfx::ImageSkia converted_image) {
  if (!converted_image.isNull())
    favicon_monochrome_cache_[app_id] = converted_image;

  if (favicon_monochrome_read_callback_)
    favicon_monochrome_read_callback_.Run(app_id);
}

}  // namespace web_app
