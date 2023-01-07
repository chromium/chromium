// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/icon_util.h"
#endif

using content::BrowserThread;

namespace web_app {

namespace {

#if BUILDFLAG(IS_MAC)
const int kDesiredIconSizesForShortcut[] = {16, 32, 128, 256, 512};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredIconSizesForShortcut[] = {16, 32, 48, 128, 256, 512};
#elif BUILDFLAG(IS_WIN)
const int* kDesiredIconSizesForShortcut = IconUtil::kIconDimensions;
#else
const int kDesiredIconSizesForShortcut[] = {32};
#endif

#if BUILDFLAG(IS_WIN)
base::LazyThreadPoolCOMSTATaskRunner g_shortcuts_task_runner =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
base::LazyThreadPoolSequencedTaskRunner g_shortcuts_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
#endif

size_t GetNumDesiredIconSizesForShortcut() {
#if BUILDFLAG(IS_WIN)
  return IconUtil::kNumIconDimensions;
#else
  return std::size(kDesiredIconSizesForShortcut);
#endif
}

void DeleteShortcutInfoOnUIThread(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  ResultCallback callback,
                                  Result result) {
  shortcut_info.reset();
  if (callback)
    std::move(callback).Run(result);
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
    DeleteShortcutsCallback callback,
    const ShortcutInfo& shortcut_info) {
  internals::DeletePlatformShortcuts(shortcut_data_path, shortcut_info,
                                     content::GetUIThreadTaskRunner({}),
                                     std::move(callback));
}

void DeleteMultiProfileShortcutsForAppAndPostCallback(const std::string& app_id,
                                                      ResultCallback callback) {
  internals::DeleteMultiProfileShortcutsForApp(app_id);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

}  // namespace

ShortcutInfo::ShortcutInfo() = default;

ShortcutInfo::~ShortcutInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<ShortcutInfo> BuildShortcutInfoWithoutFavicon(
    const AppId& app_id,
    const GURL& start_url,
    const base::FilePath& profile_path,
    const std::string& profile_name,
    const proto::WebAppOsIntegrationState& state) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->extension_id = app_id;
  shortcut_info->url = start_url;
  DCHECK(state.has_shortcut());
  const proto::ShortcutDescription& shortcut_state = state.shortcut();
  DCHECK(shortcut_state.has_title());
  shortcut_info->title = base::UTF8ToUTF16(shortcut_state.title());
  DCHECK(shortcut_state.has_description());
  shortcut_info->description = base::UTF8ToUTF16(shortcut_state.description());
  shortcut_info->profile_path = profile_path;
  shortcut_info->profile_name = profile_name;
  shortcut_info->is_multi_profile = true;

  // TODO(https://crbug.com/1295044): Add file handlers.

  if (state.has_protocols_handled()) {
    for (const auto& protocol_handler : state.protocols_handled().protocols()) {
      DCHECK(protocol_handler.has_protocol());
      if (protocol_handler.has_protocol() &&
          !protocol_handler.protocol().empty()) {
        shortcut_info->protocol_handlers.emplace(protocol_handler.protocol());
      }
    }
  }

  // TODO(https://crbug.com/1295044): Add shortcut menu infos.

  // TODO(https://crbug.com/1295044): Add mac's file handlers per profile.

  return shortcut_info;
}

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

  if (!app_id.empty()) {
    return app_data_dir.AppendASCII(GenerateApplicationNameFromAppId(app_id));
  }

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType host_path(base::UTF8ToWide(host));
  base::FilePath::StringType scheme_port_path(base::UTF8ToWide(scheme_port));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::FilePath::StringType host_path(host);
  base::FilePath::StringType scheme_port_path(scheme_port);
#else
#error "Unknown platform"
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(
          [](std::unique_ptr<ShortcutInfo> shortcut_info) {
            // This lambda is to own and delete the shortcut info.
            shortcut_info.reset();
          },
          std::move(shortcut_info)));
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

void ScheduleDeleteMultiProfileShortcutsForApp(const std::string& app_id,
                                               ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetShortcutIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteMultiProfileShortcutsForAppAndPostCallback, app_id,
                     std::move(callback)));
}

void PostShortcutIOTaskAndReplyWithResult(
    base::OnceCallback<Result(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(&DeleteShortcutInfoOnUIThread, std::move(shortcut_info),
                     std::move(reply)));
}

scoped_refptr<base::SequencedTaskRunner> GetShortcutIOTaskRunner() {
  return g_shortcuts_task_runner.Get();
}

base::FilePath GetShortcutDataDir(const ShortcutInfo& shortcut_info) {
  return GetOsIntegrationResourcesDirectoryForApp(shortcut_info.profile_path,
                                                  shortcut_info.extension_id,
                                                  shortcut_info.url);
}

#if !BUILDFLAG(IS_MAC)
void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  // Multi-profile shortcuts exist only on macOS.
  NOTREACHED();
}
#endif

}  // namespace internals
}  // namespace web_app
