// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/adapters.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

namespace web_app {

namespace {

struct IconId {
  IconId(AppId app_id, IconPurpose purpose, SquareSizePx size)
      : app_id(std::move(app_id)), purpose(purpose), size(size) {}
  ~IconId() = default;

  AppId app_id;
  IconPurpose purpose;
  SquareSizePx size;
};

// Returns false if directory doesn't exist or it is not writable.
bool CreateDirectoryIfNotExists(FileUtilsWrapper* utils,
                                const base::FilePath& path) {
  if (utils->PathExists(path)) {
    if (!utils->DirectoryExists(path)) {
      LOG(ERROR) << "Not a directory: " << path.value();
      return false;
    }
    if (!utils->PathIsWritable(path)) {
      LOG(ERROR) << "Can't write to path: " << path.value();
      return false;
    }
    // This is a directory we can write to.
    return true;
  }

  // Directory doesn't exist, so create it.
  if (!utils->CreateDirectory(path)) {
    LOG(ERROR) << "Could not create directory: " << path.value();
    return false;
  }
  return true;
}

// This is a private implementation detail of WebAppIconManager, where and how
// to store icon files.
base::FilePath GetAppIconsDirectory(
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
base::FilePath GetAppShortcutsMenuIconsDirectory(
    const base::FilePath& app_manifest_resources_directory,
    IconPurpose purpose) {
  constexpr base::FilePath::CharType kShortcutsMenuIconsDirectoryName[] =
      FILE_PATH_LITERAL("Shortcuts Menu Icons");

  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMonochromeDirectoryName[] =
          FILE_PATH_LITERAL("Monochrome");
  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMaskableDirectoryName[] =
          FILE_PATH_LITERAL("Maskable");

  base::FilePath shortcuts_icons_directory =
      app_manifest_resources_directory.Append(kShortcutsMenuIconsDirectoryName);

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

bool WriteIcon(FileUtilsWrapper* utils,
               const base::FilePath& icons_dir,
               const SkBitmap& bitmap) {
  DCHECK_NE(bitmap.colorType(), kUnknown_SkColorType);
  DCHECK_EQ(bitmap.width(), bitmap.height());
  base::FilePath icon_file =
      icons_dir.AppendASCII(base::StringPrintf("%i.png", bitmap.width()));

  std::vector<unsigned char> image_data;
  const bool discard_transparency = false;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                         &image_data)) {
    LOG(ERROR) << "Could not encode icon data for file " << icon_file;
    return false;
  }

  const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
  int size = base::checked_cast<int>(image_data.size());
  if (utils->WriteFile(icon_file, image_data_ptr, size) != size) {
    LOG(ERROR) << "Could not write icon file: " << icon_file;
    return false;
  }

  return true;
}

bool WriteIcons(FileUtilsWrapper* utils,
                const base::FilePath& icons_dir,
                const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  if (!utils->CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return false;
  }

  for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
       icon_bitmaps) {
    if (!WriteIcon(utils, icons_dir, icon_bitmap.second))
      return false;
  }

  return true;
}

// Writes shortcuts menu icons files to the Shortcut Icons directory. Creates a
// new directory per shortcut item using its index in the vector.
bool WriteShortcutsMenuIcons(
    FileUtilsWrapper* utils,
    const base::FilePath& app_manifest_resources_directory,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  // TODO(crbug.com/1114638): Write monochrome icons too.
  std::array<IconPurpose, 2> purposes = {IconPurpose::ANY,
                                         IconPurpose::MASKABLE};
  for (IconPurpose purpose : purposes) {
    const base::FilePath shortcuts_menu_icons_dir =
        GetAppShortcutsMenuIconsDirectory(app_manifest_resources_directory,
                                          purpose);
    if (!utils->CreateDirectory(shortcuts_menu_icons_dir))
      return false;

    int shortcut_index = -1;
    for (const IconBitmaps& icon_bitmaps : shortcuts_menu_icon_bitmaps) {
      ++shortcut_index;
      const std::map<SquareSizePx, SkBitmap>& bitmaps =
          icon_bitmaps.GetBitmapsForPurpose(purpose);
      if (bitmaps.empty())
        continue;

      const base::FilePath shortcuts_menu_icon_dir =
          shortcuts_menu_icons_dir.AppendASCII(
              base::NumberToString(shortcut_index));
      if (!utils->CreateDirectory(shortcuts_menu_icon_dir))
        return false;

      for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
           bitmaps) {
        if (!WriteIcon(utils, shortcuts_menu_icon_dir, icon_bitmap.second))
          return false;
      }
    }
  }
  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool WriteDataBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                       const base::FilePath& web_apps_directory,
                       const AppId& app_id,
                       const IconBitmaps& icon_bitmaps) {
  // Create the temp directory under the web apps root.
  // This guarantees it is on the same file system as the WebApp's eventual
  // install target.
  base::FilePath temp_dir = GetWebAppsTempDirectory(web_apps_directory);
  if (!CreateDirectoryIfNotExists(utils.get(), temp_dir)) {
    LOG(ERROR) << "Could not create or write to WebApps temporary directory in "
                  "profile.";
    return false;
  }

  base::ScopedTempDir app_temp_dir;
  if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir)) {
    LOG(ERROR) << "Could not create temporary WebApp directory.";
    return false;
  }

  if (!WriteIcons(
          utils.get(),
          GetAppIconsDirectory(app_temp_dir.GetPath(), IconPurpose::ANY),
          icon_bitmaps.any)) {
    return false;
  }
  // TODO (crbug.com/1114638): Write Monochrome icons here.
  if (!WriteIcons(
          utils.get(),
          GetAppIconsDirectory(app_temp_dir.GetPath(), IconPurpose::MASKABLE),
          icon_bitmaps.maskable)) {
    return false;
  }

  base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_directory);
  if (!CreateDirectoryIfNotExists(utils.get(), manifest_resources_directory)) {
    LOG(ERROR) << "Could not create Manifest Resources directory.";
    return false;
  }

  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);

  // Try to delete the destination. Needed for update. Ignore the result.
  utils->DeleteFileRecursively(app_dir);

  // Commit: move whole app data dir to final destination in one mv operation.
  if (!utils->Move(app_temp_dir.GetPath(), app_dir)) {
    LOG(ERROR) << "Could not move temp WebApp directory to final destination.";
    return false;
  }

  app_temp_dir.Take();
  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool WriteShortcutsMenuIconsDataBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  if (shortcuts_menu_icon_bitmaps.empty())
    return false;

  // Create the temp directory under the web apps root.
  // This guarantees it is on the same file system as the WebApp's eventual
  // install target.
  base::FilePath temp_dir = GetWebAppsTempDirectory(web_apps_directory);
  if (!CreateDirectoryIfNotExists(utils.get(), temp_dir))
    return false;

  base::ScopedTempDir app_temp_dir;
  if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir))
    return false;


  base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_directory);
  if (!CreateDirectoryIfNotExists(utils.get(), manifest_resources_directory))
    return false;

  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);

  // Create app_dir if it doesn't already exist. We'll need this for
  // WriteShortcutsMenuIconsData unittests.
  if (!CreateDirectoryIfNotExists(utils.get(), app_dir))
    return false;

  if (!WriteShortcutsMenuIcons(utils.get(), app_temp_dir.GetPath(),
                               shortcuts_menu_icon_bitmaps)) {
    return false;
  }

  {
    base::FilePath shortcuts_menu_icons_dir =
        GetAppShortcutsMenuIconsDirectory(app_dir, IconPurpose::ANY);

    // Delete the destination. Needed for update. Return if destination isn't
    // clear.
    if (!utils->DeleteFileRecursively(shortcuts_menu_icons_dir))
      return false;

    // Commit: move whole shortcuts menu icons data dir to final destination in
    // one mv operation.
    if (!utils->Move(GetAppShortcutsMenuIconsDirectory(app_temp_dir.GetPath(),
                                                       IconPurpose::ANY),
                     shortcuts_menu_icons_dir))
      return false;
  }

  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool DeleteDataBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                        const base::FilePath& web_apps_directory,
                        const AppId& app_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);

  return utils->DeleteFileRecursively(app_dir);
}

base::FilePath GetIconFileName(const base::FilePath& web_apps_directory,
                               const IconId& icon_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, icon_id.app_id);
  base::FilePath icons_dir = GetAppIconsDirectory(app_dir, icon_id.purpose);

  return icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_id.size));
}

base::FilePath GetManifestResourcesShortcutsMenuIconFileName(
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    IconPurpose purpose,
    int index,
    int icon_size_px) {
  const base::FilePath manifest_app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);
  const base::FilePath manifest_shortcuts_menu_icons_dir =
      GetAppShortcutsMenuIconsDirectory(manifest_app_dir, purpose);
  const base::FilePath manifest_shortcuts_menu_icon_dir =
      manifest_shortcuts_menu_icons_dir.AppendASCII(
          base::NumberToString(index));

  return manifest_shortcuts_menu_icon_dir.AppendASCII(
      base::NumberToString(icon_size_px) + ".png");
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
SkBitmap ReadIconBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                          const base::FilePath& web_apps_directory,
                          const IconId& icon_id) {
  base::FilePath icon_file = GetIconFileName(web_apps_directory, icon_id);

  auto icon_data = base::MakeRefCounted<base::RefCountedString>();

  if (!utils->ReadFileToString(icon_file, &icon_data->data())) {
    LOG(ERROR) << "Could not read icon file: " << icon_file;
    return SkBitmap();
  }

  SkBitmap bitmap;

  if (!gfx::PNGCodec::Decode(icon_data->front(), icon_data->size(), &bitmap)) {
    LOG(ERROR) << "Could not decode icon data for file " << icon_file;
    return SkBitmap();
  }

  return bitmap;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
SkBitmap ReadShortcutsMenuIconBlocking(FileUtilsWrapper* utils,
                                       const base::FilePath& web_apps_directory,
                                       const AppId& app_id,
                                       IconPurpose purpose,
                                       int index,
                                       int icon_size_px) {
  base::FilePath manifest_shortcuts_menu_icon_file =
      GetManifestResourcesShortcutsMenuIconFileName(
          web_apps_directory, app_id, purpose, index, icon_size_px);

  std::string icon_data;

  if (!utils->ReadFileToString(manifest_shortcuts_menu_icon_file, &icon_data)) {
    return SkBitmap();
  }

  SkBitmap bitmap;

  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(icon_data.c_str()),
          icon_data.size(), &bitmap)) {
    return SkBitmap();
  }

  return bitmap;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty map if any errors occurred.
std::map<SquareSizePx, SkBitmap> ReadIconAndResizeBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id,
    SquareSizePx target_icon_size_px) {
  std::map<SquareSizePx, SkBitmap> result;

  SkBitmap source = ReadIconBlocking(utils, web_apps_directory, icon_id);
  if (source.empty())
    return result;

  SkBitmap target;

  if (icon_id.size != target_icon_size_px) {
    target = skia::ImageOperations::Resize(
        source, skia::ImageOperations::RESIZE_BEST, target_icon_size_px,
        target_icon_size_px);
  } else {
    target = source;
  }

  result[target_icon_size_px] = target;
  return result;
}

// Performs blocking I/O. May be called on another thread.
std::map<SquareSizePx, SkBitmap> ReadIconsBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    IconPurpose purpose,
    const std::vector<SquareSizePx>& icon_sizes) {
  std::map<SquareSizePx, SkBitmap> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    IconId icon_id(app_id, purpose, icon_size_px);
    SkBitmap bitmap = ReadIconBlocking(utils, web_apps_directory, icon_id);
    if (!bitmap.empty())
      result[icon_size_px] = bitmap;
  }

  return result;
}

IconBitmaps ReadAllIconsBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::map<IconPurpose, std::vector<SquareSizePx>>&
        icon_purposes_to_sizes) {
  IconBitmaps result;

  for (const auto& purpose_sizes : icon_purposes_to_sizes) {
    std::map<SquareSizePx, SkBitmap> read_icons =
        ReadIconsBlocking(utils, web_apps_directory, app_id,
                          purpose_sizes.first, purpose_sizes.second);
    result.SetBitmapsForPurpose(purpose_sizes.first, std::move(read_icons));
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
ShortcutsMenuIconBitmaps ReadShortcutsMenuIconsBlocking(
    FileUtilsWrapper* utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::vector<IconSizes>& shortcuts_menu_icons_sizes) {
  ShortcutsMenuIconBitmaps results;
  int curr_index = 0;
  for (const auto& icon_sizes : shortcuts_menu_icons_sizes) {
    IconBitmaps result;

    // TODO(crbug.com/1114638): Read monochrome icons too.
    std::array<IconPurpose, 2> purposes = {IconPurpose::ANY,
                                           IconPurpose::MASKABLE};
    for (IconPurpose purpose : purposes) {
      std::map<SquareSizePx, SkBitmap> bitmaps;

      for (SquareSizePx icon_size_px : icon_sizes.GetSizesForPurpose(purpose)) {
        SkBitmap bitmap =
            ReadShortcutsMenuIconBlocking(utils, web_apps_directory, app_id,
                                          purpose, curr_index, icon_size_px);
        if (!bitmap.empty())
          bitmaps[icon_size_px] = bitmap;
      }

      result.SetBitmapsForPurpose(purpose, std::move(bitmaps));
    }

    ++curr_index;
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.push_back(std::move(result));
  }
  return results;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty vector if any errors occurred.
std::vector<uint8_t> ReadCompressedIconBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id) {
  base::FilePath icon_file = GetIconFileName(web_apps_directory, icon_id);

  std::string icon_data;

  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    LOG(ERROR) << "Could not read icon file: " << icon_file;
    return std::vector<uint8_t>{};
  }

  // Copy data: we can't std::move std::string into std::vector.
  return std::vector<uint8_t>(icon_data.begin(), icon_data.end());
}

void WrapReadCompressedIconWithPurposeCallback(
    AppIconManager::ReadCompressedIconWithPurposeCallback callback,
    IconPurpose purpose,
    std::vector<uint8_t> data) {
  std::move(callback).Run(purpose, std::move(data));
}

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

}  // namespace

WebAppIconManager::WebAppIconManager(Profile* profile,
                                     WebAppRegistrar& registrar,
                                     std::unique_ptr<FileUtilsWrapper> utils)
    : registrar_(registrar), utils_(std::move(utils)) {
  web_apps_directory_ = GetWebAppsRootDirectory(profile);
}

WebAppIconManager::~WebAppIconManager() = default;

void WebAppIconManager::WriteData(AppId app_id,
                                  IconBitmaps icon_bitmaps,
                                  WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id), std::move(icon_bitmaps)),
      std::move(callback));
}

void WebAppIconManager::WriteShortcutsMenuIconsData(
    AppId app_id,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps,
    WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteShortcutsMenuIconsDataBlocking, utils_->Clone(),
                     web_apps_directory_, std::move(app_id),
                     std::move(shortcuts_menu_icon_bitmaps)),
      std::move(callback));
}

void WebAppIconManager::DeleteData(AppId app_id, WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(DeleteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id)),
      std::move(callback));
}

void WebAppIconManager::Start() {
  for (const AppId& app_id : registrar_.GetAppIds()) {
    ReadFavicon(app_id);
  }
  registrar_observer_.Add(&registrar_);
}

void WebAppIconManager::Shutdown() {}

bool WebAppIconManager::HasIcons(const AppId& app_id,
                                 IconPurpose purpose,
                                 const SortedSizesPx& icon_sizes) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return false;

  return base::ranges::includes(web_app->downloaded_icon_sizes(purpose),
                                icon_sizes);
}

base::Optional<AppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchBigger(const AppId& app_id,
                                       const std::vector<IconPurpose>& purposes,
                                       SquareSizePx min_size) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  // Must iterate through purposes in order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from smallest to largest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : sizes) {
      if (size >= min_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return base::nullopt;
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
                                  ReadIconsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(HasIcons(app_id, purpose, icon_sizes));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          ReadIconsBlocking, utils_->Clone(), web_apps_directory_, app_id,
          purpose,
          std::vector<SquareSizePx>(icon_sizes.begin(), icon_sizes.end())),
      std::move(callback));
}

void WebAppIconManager::ReadAllIcons(const AppId& app_id,
                                     ReadIconBitmapsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(IconBitmaps());
    return;
  }

  std::map<IconPurpose, std::vector<SquareSizePx>> icon_purposes_to_sizes;
  const SortedSizesPx& sizes_any =
      web_app->downloaded_icon_sizes(IconPurpose::ANY);
  icon_purposes_to_sizes[IconPurpose::ANY] =
      std::vector<SquareSizePx>(sizes_any.begin(), sizes_any.end());
  const SortedSizesPx& sizes_maskable =
      web_app->downloaded_icon_sizes(IconPurpose::MASKABLE);
  icon_purposes_to_sizes[IconPurpose::MASKABLE] =
      std::vector<SquareSizePx>(sizes_maskable.begin(), sizes_maskable.end());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadAllIconsBlocking, utils_->Clone(), web_apps_directory_,
                     app_id, std::move(icon_purposes_to_sizes)),
      std::move(callback));
}

void WebAppIconManager::ReadAllShortcutsMenuIcons(
    const AppId& app_id,
    ReadShortcutsMenuIconsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(ShortcutsMenuIconBitmaps{});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadShortcutsMenuIconsBlocking, utils_.get(),
                     web_apps_directory_, app_id,
                     web_app->downloaded_shortcuts_menu_icons_sizes()),
      std::move(callback));
}

void WebAppIconManager::ReadSmallestIcon(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadIconWithPurposeCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  DCHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadIconCallback wrapped = base::BindOnce(
      WrapReadIconWithPurposeCallback, std::move(callback), best_icon->purpose);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(icon_id)),
      std::move(wrapped));
}

void WebAppIconManager::ReadSmallestCompressedIcon(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadCompressedIconWithPurposeCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  DCHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadCompressedIconCallback wrapped =
      base::BindOnce(WrapReadCompressedIconWithPurposeCallback,
                     std::move(callback), best_icon->purpose);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadCompressedIconBlocking, utils_->Clone(),
                     web_apps_directory_, std::move(icon_id)),
      std::move(wrapped));
}

SkBitmap WebAppIconManager::GetFavicon(const AppId& app_id) const {
  auto iter = favicon_cache_.find(app_id);
  if (iter == favicon_cache_.end())
    return SkBitmap();
  return iter->second;
}

void WebAppIconManager::OnWebAppInstalled(const AppId& app_id) {
  ReadFavicon(app_id);
}

void WebAppIconManager::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void WebAppIconManager::ReadIconAndResize(const AppId& app_id,
                                          IconPurpose purpose,
                                          SquareSizePx desired_icon_size,
                                          ReadIconsCallback callback) const {
  base::Optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, {purpose}, desired_icon_size);
  if (!best_icon) {
    best_icon = FindIconMatchSmaller(app_id, {purpose}, desired_icon_size);
  }

  if (!best_icon) {
    std::move(callback).Run(std::map<SquareSizePx, SkBitmap>());
    return;
  }

  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconAndResizeBlocking, utils_->Clone(),
                     web_apps_directory_, std::move(icon_id),
                     desired_icon_size),
      std::move(callback));
}

void WebAppIconManager::SetFaviconReadCallbackForTesting(
    FaviconReadCallback callback) {
  favicon_read_callback_ = std::move(callback);
}

base::Optional<AppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchSmaller(
    const AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx max_size) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  // Must check purposes in the order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from largest to smallest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : base::Reversed(sizes)) {
      if (size <= max_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return base::nullopt;
}

void WebAppIconManager::ReadFavicon(const AppId& app_id) {
  if (!HasSmallestIcon(app_id, {IconPurpose::ANY}, gfx::kFaviconSize))
    return;

  ReadIconAndResize(app_id, IconPurpose::ANY, gfx::kFaviconSize,
                    base::BindOnce(&WebAppIconManager::OnReadFavicon,
                                   weak_ptr_factory_.GetWeakPtr(), app_id));
}

void WebAppIconManager::OnReadFavicon(
    const AppId& app_id,
    const std::map<SquareSizePx, SkBitmap> icons) {
  const auto it = icons.find(gfx::kFaviconSize);
  if (it != icons.end())
    favicon_cache_[app_id] = it->second;
  if (favicon_read_callback_)
    favicon_read_callback_.Run(app_id);
}

}  // namespace web_app
