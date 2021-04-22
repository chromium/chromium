// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"

#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/service_process_host.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"

namespace {

ChromeCaptureModeDelegate* g_instance = nullptr;

ScreenshotArea ConvertToScreenshotArea(const aura::Window* window,
                                       const gfx::Rect& bounds) {
  return window->IsRootWindow()
             ? ScreenshotArea::CreateForPartialWindow(window, bounds)
             : ScreenshotArea::CreateForWindow(window);
}

bool IsScreenCaptureDisabledByPolicy() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kDisableScreenshots);
}

}  // namespace

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ChromeCaptureModeDelegate::~ChromeCaptureModeDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ChromeCaptureModeDelegate* ChromeCaptureModeDelegate::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void ChromeCaptureModeDelegate::SetIsScreenCaptureLocked(bool locked) {
  is_screen_capture_locked_ = locked;
  if (is_screen_capture_locked_)
    InterruptVideoRecordingIfAny();
}

void ChromeCaptureModeDelegate::InterruptVideoRecordingIfAny() {
  if (interrupt_video_recording_callback_)
    std::move(interrupt_video_recording_callback_).Run();
}

base::FilePath ChromeCaptureModeDelegate::GetScreenCaptureDir() const {
  if (chromeos::LoginState::Get()->IsUserLoggedIn()) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    // We use the default downloads directory instead of the one that can be
    // configured from the browser's settings, since it can point to an invalid
    // location, which the browser handles by prompting the user to select
    // another one when accessed, but Capture Mode doesn't have this capability.
    // We also decided that this browser setting should not affect when the OS
    // saves the captured files. https://crbug.com/1192406.
    return download_prefs->GetDefaultDownloadDirectoryForProfile();
  }
  base::FilePath tmp_dir;
  if (!base::GetTempDir(&tmp_dir))
    LOG(ERROR) << "Failed to find temporary directory.";
  return tmp_dir;
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

  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(
          profile->GetOriginalProfile());
  apps::mojom::FilePathsPtr file_paths_ptr =
      apps::mojom::FilePaths::New(std::vector<base::FilePath>({file_path}));

  // open the image with Essential App: Backlight.
  proxy->LaunchAppWithFiles(
      web_app::kMediaAppId,
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

bool ChromeCaptureModeDelegate::IsCaptureModeInitRestrictedByDlp() const {
  return policy::DlpContentManager::Get()->IsCaptureModeInitRestricted();
}

bool ChromeCaptureModeDelegate::IsCaptureAllowedByDlp(
    const aura::Window* window,
    const gfx::Rect& bounds,
    bool for_video) const {
  policy::DlpContentManager* dlp_content_manager =
      policy::DlpContentManager::Get();
  const ScreenshotArea area = ConvertToScreenshotArea(window, bounds);
  return for_video ? !dlp_content_manager->IsVideoCaptureRestricted(area)
                   : !dlp_content_manager->IsScreenshotRestricted(area);
}

bool ChromeCaptureModeDelegate::IsCaptureAllowedByPolicy() const {
  return !is_screen_capture_locked_ && !IsScreenCaptureDisabledByPolicy();
}

void ChromeCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {
  // The order here matters, since DlpContentManager::OnVideoCaptureStarted()
  // may call InterruptVideoRecordingIfAny() right away, so the callback must be
  // set first.
  interrupt_video_recording_callback_ = std::move(stop_callback);
  policy::DlpContentManager::Get()->OnVideoCaptureStarted(
      ConvertToScreenshotArea(window, bounds));
}

void ChromeCaptureModeDelegate::StopObservingRestrictedContent() {
  interrupt_video_recording_callback_.Reset();
  policy::DlpContentManager::Get()->OnVideoCaptureStopped();
}

mojo::Remote<recording::mojom::RecordingService>
ChromeCaptureModeDelegate::LaunchRecordingService() {
  return content::ServiceProcessHost::Launch<
      recording::mojom::RecordingService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Recording Service")
          .Pass());
}

void ChromeCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  content::GetAudioService().BindStreamFactory(std::move(receiver));
}

void ChromeCaptureModeDelegate::OnSessionStateChanged(bool started) {
  is_session_active_ = started;
}
