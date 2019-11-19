// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_app {

namespace {

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

constexpr base::FilePath::CharType kIconsDirectoryName[] =
    FILE_PATH_LITERAL("Icons");

base::FilePath GetAppDirectory(const base::FilePath& web_apps_directory,
                               const AppId& app_id) {
  return web_apps_directory.AppendASCII(app_id);
}

base::FilePath GetTempDir(FileUtilsWrapper* utils,
                          const base::FilePath& web_apps_dir) {
  // Create the temp directory as a sub-directory of the WebApps directory.
  // This guarantees it is on the same file system as the WebApp's eventual
  // install target.
  base::FilePath temp_path = web_apps_dir.Append(kTempDirectoryName);
  if (utils->PathExists(temp_path)) {
    if (!utils->DirectoryExists(temp_path)) {
      LOG(ERROR) << "Not a directory: " << temp_path.value();
      return base::FilePath();
    }
    if (!utils->PathIsWritable(temp_path)) {
      LOG(ERROR) << "Can't write to path: " << temp_path.value();
      return base::FilePath();
    }
    // This is a directory we can write to.
    return temp_path;
  }

  // Directory doesn't exist, so create it.
  if (!utils->CreateDirectory(temp_path)) {
    LOG(ERROR) << "Could not create directory: " << temp_path.value();
    return base::FilePath();
  }
  return temp_path;
}

bool WriteIcon(FileUtilsWrapper* utils,
               const base::FilePath& icons_dir,
               const WebApplicationIconInfo& icon_info) {
  base::FilePath icon_file =
      icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_info.width));

  std::vector<unsigned char> image_data;
  const bool discard_transparency = false;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(icon_info.data, discard_transparency,
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
                const base::FilePath& app_dir,
                const std::vector<WebApplicationIconInfo>& icon_infos) {
  const base::FilePath icons_dir = app_dir.Append(kIconsDirectoryName);
  if (!utils->CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return false;
  }

  for (const WebApplicationIconInfo& icon_info : icon_infos) {
    // Skip unfetched bitmaps.
    if (icon_info.data.colorType() == kUnknown_SkColorType)
      continue;

    if (!WriteIcon(utils, icons_dir, icon_info))
      return false;
  }

  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool WriteDataBlocking(std::unique_ptr<FileUtilsWrapper> utils,
                       base::FilePath web_apps_directory,
                       AppId app_id,
                       std::vector<WebApplicationIconInfo> icon_infos) {
  const base::FilePath temp_dir = GetTempDir(utils.get(), web_apps_directory);
  if (temp_dir.empty()) {
    LOG(ERROR)
        << "Could not get path to WebApps temporary directory in profile.";
    return false;
  }

  base::ScopedTempDir app_temp_dir;
  if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir)) {
    LOG(ERROR) << "Could not create temporary WebApp directory.";
    return false;
  }

  if (!WriteIcons(utils.get(), app_temp_dir.GetPath(), icon_infos))
    return false;

  // Commit: move whole app data dir to final destination in one mv operation.
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);
  if (!utils->Move(app_temp_dir.GetPath(), app_dir)) {
    LOG(ERROR) << "Could not move temp WebApp directory to final destination.";
    return false;
  }

  app_temp_dir.Take();
  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool DeleteDataBlocking(std::unique_ptr<FileUtilsWrapper> utils,
                        base::FilePath web_apps_directory,
                        AppId app_id) {
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);
  return utils->DeleteFileRecursively(app_dir);
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
SkBitmap ReadIconBlocking(std::unique_ptr<FileUtilsWrapper> utils,
                          base::FilePath web_apps_directory,
                          AppId app_id,
                          int icon_size_px) {
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);
  const base::FilePath icons_dir = app_dir.Append(kIconsDirectoryName);

  base::FilePath icon_file =
      icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_size_px));

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

constexpr base::TaskTraits kTaskTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

WebAppIconManager::WebAppIconManager(Profile* profile,
                                     WebAppRegistrar& registrar,
                                     std::unique_ptr<FileUtilsWrapper> utils)
    : registrar_(registrar), utils_(std::move(utils)) {
  web_apps_directory_ = GetWebAppsDirectory(profile);
}

WebAppIconManager::~WebAppIconManager() = default;

void WebAppIconManager::WriteData(
    AppId app_id,
    std::vector<WebApplicationIconInfo> icon_infos,
    WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id), std::move(icon_infos)),
      std::move(callback));
}

void WebAppIconManager::DeleteData(AppId app_id, WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(DeleteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id)),
      std::move(callback));
}

bool WebAppIconManager::ReadIcon(const AppId& app_id,
                                 int icon_size_in_px,
                                 ReadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return false;

  for (const WebApp::IconInfo& icon_info : web_app->icons()) {
    if (icon_info.size_in_px == icon_size_in_px) {
      ReadIconInternal(app_id, icon_size_in_px, std::move(callback));
      return true;
    }
  }

  return false;
}

bool WebAppIconManager::ReadSmallestIcon(const AppId& app_id,
                                         int icon_size_in_px,
                                         ReadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return false;

  int best_size_in_px = std::numeric_limits<int>::max();
  for (const WebApp::IconInfo& icon_info : web_app->icons()) {
    if (icon_info.size_in_px >= icon_size_in_px &&
        icon_info.size_in_px < best_size_in_px) {
      best_size_in_px = icon_info.size_in_px;
    }
  }

  if (best_size_in_px == std::numeric_limits<int>::max())
    return false;

  ReadIconInternal(app_id, best_size_in_px, std::move(callback));
  return true;
}

void WebAppIconManager::ReadIconInternal(const AppId& app_id,
                                         int icon_size_in_px,
                                         ReadIconCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconBlocking, utils_->Clone(), web_apps_directory_,
                     app_id, icon_size_in_px),
      std::move(callback));
}

}  // namespace web_app
