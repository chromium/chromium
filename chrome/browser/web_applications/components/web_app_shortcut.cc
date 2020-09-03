// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut.h"

#include <functional>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "ui/gfx/icon_util.h"
#endif

using content::BrowserThread;

namespace web_app {

namespace {

#if defined(OS_MAC)
const int kDesiredIconSizesForShortcut[] = {16, 32, 128, 256, 512};
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredIconSizesForShortcut[] = {16, 32, 48, 128, 256, 512};
#elif defined(OS_WIN)
const int* kDesiredIconSizesForShortcut = IconUtil::kIconDimensions;
#else
const int kDesiredIconSizesForShortcut[] = {32};
#endif

size_t GetNumDesiredIconSizesForShortcut() {
#if defined(OS_WIN)
  return IconUtil::kNumIconDimensions;
#else
  return base::size(kDesiredIconSizesForShortcut);
#endif
}

void DeleteShortcutInfoOnUIThread(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  base::OnceClosure callback) {
  shortcut_info.reset();
  if (callback)
    std::move(callback).Run();
}

void CreatePlatformShortcutsAndPostCallback(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason,
    CreateShortcutsCallback callback,
    const ShortcutInfo& shortcut_info) {
  bool shortcut_created = internals::CreatePlatformShortcuts(
      shortcut_data_path, creation_locations, creation_reason, shortcut_info);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), shortcut_created));
}

void DeletePlatformShortcutsAndPostCallback(
    const base::FilePath& shortcut_data_path,
    CreateShortcutsCallback callback,
    const ShortcutInfo& shortcut_info) {
  bool shortcut_deleted =
      internals::DeletePlatformShortcuts(shortcut_data_path, shortcut_info);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), shortcut_deleted));
}

}  // namespace

ShortcutInfo::ShortcutInfo() = default;

ShortcutInfo::~ShortcutInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ShortcutLocations::ShortcutLocations()
    : on_desktop(false),
      applications_menu_location(APP_MENU_LOCATION_NONE),
      in_quick_launch_bar(false),
      in_startup(false) {}

std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info) {
  // TODO(loyso): Remove this empty()/non-empty difference.
  if (shortcut_info.extension_id.empty())
    return GenerateApplicationNameFromURL(shortcut_info.url);

  return GenerateApplicationNameFromAppId(shortcut_info.extension_id);
}

base::FilePath GetOsIntegrationResourcesDirectoryForApp(
    const base::FilePath& profile_path,
    const std::string& app_id,
    const GURL& url) {
  DCHECK(!profile_path.empty());
  base::FilePath app_data_dir(profile_path.Append(chrome::kWebAppDirname));

  if (!app_id.empty())
    return app_data_dir.AppendASCII(GenerateApplicationNameFromAppId(app_id));

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if defined(OS_WIN)
  base::FilePath::StringType host_path(base::UTF8ToUTF16(host));
  base::FilePath::StringType scheme_port_path(base::UTF8ToUTF16(scheme_port));
#elif defined(OS_POSIX)
  base::FilePath::StringType host_path(host);
  base::FilePath::StringType scheme_port_path(scheme_port);
#endif

  return app_data_dir.Append(host_path).Append(scheme_port_path);
}

base::span<const int> GetDesiredIconSizesForShortcut() {
  return base::span<const int>(kDesiredIconSizesForShortcut,
                               GetNumDesiredIconSizesForShortcut());
}

gfx::ImageSkia CreateDefaultApplicationIcon(int size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/860581): Create web_app_browser_resources.grd with the
  // default app icon. Remove dependency on extensions_browser_resources.h and
  // use IDR_WEB_APP_DEFAULT_ICON here.
  gfx::Image default_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_APP_DEFAULT_ICON);
  SkBitmap bmp = skia::ImageOperations::Resize(
      *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, size,
      size);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bmp);
  // We are on the UI thread, and this image can be used from the FILE thread,
  // for creating shortcut icon files.
  image_skia.MakeThreadSafe();

  return image_skia;
}

namespace internals {

void PostShortcutIOTask(base::OnceCallback<void(const ShortcutInfo&)> task,
                        std::unique_ptr<ShortcutInfo> shortcut_info) {
  PostShortcutIOTaskAndReply(std::move(task), std::move(shortcut_info),
                             base::OnceClosure());
}

void ScheduleCreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason reason,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PostShortcutIOTask(base::BindOnce(&CreatePlatformShortcutsAndPostCallback,
                                    shortcut_data_path, creation_locations,
                                    reason, std::move(callback)),
                     std::move(shortcut_info));
}

void ScheduleDeletePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    DeleteShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PostShortcutIOTask(base::BindOnce(&DeletePlatformShortcutsAndPostCallback,
                                    shortcut_data_path, std::move(callback)),
                     std::move(shortcut_info));
}

void PostShortcutIOTaskAndReply(
    base::OnceCallback<void(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(&DeleteShortcutInfoOnUIThread, std::move(shortcut_info),
                     std::move(reply)));
}

scoped_refptr<base::TaskRunner> GetShortcutIOTaskRunner() {
  constexpr base::TaskTraits traits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

#if defined(OS_WIN)
  return base::ThreadPool::CreateCOMSTATaskRunner(
      traits, base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
  return base::ThreadPool::CreateTaskRunner(traits);
#endif
}

base::FilePath GetSanitizedFileName(const base::string16& name) {
#if defined(OS_WIN)
  base::string16 file_name = name;
#else
  std::string file_name = base::UTF16ToUTF8(name);
#endif
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
  return base::FilePath(file_name);
}

base::FilePath GetShortcutDataDir(const ShortcutInfo& shortcut_info) {
  return GetOsIntegrationResourcesDirectoryForApp(shortcut_info.profile_path,
                                                  shortcut_info.extension_id,
                                                  shortcut_info.url);
}

#if !defined(OS_MAC)
void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  // Multi-profile shortcuts exist only on macOS.
  NOTREACHED();
}
#endif

}  // namespace internals

}  // namespace web_app
