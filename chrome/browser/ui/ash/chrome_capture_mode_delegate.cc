// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"

namespace {

ScreenshotArea ConvertToScreenshotArea(const aura::Window* window,
                                       const gfx::Rect& bounds) {
  return window->IsRootWindow()
             ? ScreenshotArea::CreateForPartialWindow(window, bounds)
             : ScreenshotArea::CreateForWindow(window);
}

}  // namespace

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate() = default;

ChromeCaptureModeDelegate::~ChromeCaptureModeDelegate() = default;

base::FilePath ChromeCaptureModeDelegate::GetActiveUserDownloadsDir() const {
  DCHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(ProfileManager::GetActiveUserProfile());
  return download_prefs->DownloadPath();
}

void ChromeCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {
  platform_util::ShowItemInFolder(ProfileManager::GetActiveUserProfile(),
                                  file_path);
}

void ChromeCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      profile->GetOriginalProfile());
  apps::mojom::FilePathsPtr file_paths_ptr =
      apps::mojom::FilePaths::New(std::vector<base::FilePath>({file_path}));

  // open the image with Essential App: Backlight.
  proxy->LaunchAppWithFiles(
      chromeos::default_web_apps::kMediaAppId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*preferred_container=*/true),
      apps::mojom::LaunchSource::kFromFileManager, std::move(file_paths_ptr));
}

bool ChromeCaptureModeDelegate::Uses24HourFormat() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // TODO(afakhry): Consider moving |prefs::kUse24HourClock| to ash/public so
  // we can do this entirely in ash.
  if (profile)
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  return base::GetHourClockType() == base::k24HourClock;
}

bool ChromeCaptureModeDelegate::IsCaptureAllowed(const aura::Window* window,
                                                 const gfx::Rect& bounds,
                                                 bool for_video) const {
  policy::DlpContentManager* dlp_content_manager =
      policy::DlpContentManager::Get();
  const ScreenshotArea area = ConvertToScreenshotArea(window, bounds);
  return for_video ? !dlp_content_manager->IsVideoCaptureRestricted(area)
                   : !dlp_content_manager->IsScreenshotRestricted(area);
}

void ChromeCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {
  policy::DlpContentManager::Get()->OnVideoCaptureStarted(
      ConvertToScreenshotArea(window, bounds), std::move(stop_callback));
}

void ChromeCaptureModeDelegate::StopObservingRestrictedContent() {
  policy::DlpContentManager::Get()->OnVideoCaptureStopped();
}
