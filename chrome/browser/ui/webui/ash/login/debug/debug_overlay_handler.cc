// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/debug/debug_overlay_handler.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "ui/display/display_switches.h"
#include "ui/snapshot/snapshot.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

// A helper function which saves screenshot to the file.
void StoreScreenshot(const base::FilePath& screenshot_dir,
                     const std::string& screenshot_name,
                     scoped_refptr<base::RefCountedMemory> png_data) {
  if (!base::CreateDirectory(screenshot_dir)) {
    LOG(ERROR) << "Failed to create screenshot dir at "
               << screenshot_dir.value();
    return;
  }
  base::FilePath file_path = screenshot_dir.Append(screenshot_name);

  if (!base::WriteFile(file_path, *png_data)) {
    LOG(ERROR) << "Failed to save screenshot to " << file_path.value();
  } else {
    VLOG(1) << "Saved screenshot to " << file_path.value();
  }
}

// A helper function which invokes StoreScreenshot on TaskRunner.
void RunStoreScreenshotOnTaskRunner(
    const base::FilePath& screenshot_dir,
    const std::string& screenshot_name,
    scoped_refptr<base::RefCountedMemory> png_data) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&StoreScreenshot, screenshot_dir, screenshot_name,
                     png_data));
}

}  // namespace
// DebugOverlayHandler, public: -----------------------------------------------

DebugOverlayHandler::DebugOverlayHandler() {
  // Rules for base directory:
  // 1) If command-line switch is specified, use the directory
  // 2) else if chromeos-on-linux case create OOBE_Screenshots in user-data-dir
  // 3) else (running on real device) create OOBE_Screenshots in /tmp
  base::FilePath base_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOobeScreenshotDirectory)) {
    base_dir =
        command_line->GetSwitchValuePath(switches::kOobeScreenshotDirectory);
  } else {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      if (!base::GetTempDir(&base_dir)) {
        LOG(ERROR) << "Could not get Temp dir";
      }
    } else {
      // use user-data-dir as base directory when running chromeos-on-linux.
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &base_dir)) {
        LOG(ERROR) << "User Data Directory not found";
      }
    }
    base_dir = base_dir.Append("OOBE_Screenshots");
  }

  add_resolution_to_filename_ =
      command_line->HasSwitch(::switches::kHostWindowBounds);

  screenshot_dir_ = base_dir.Append(base::UnlocalizedTimeFormatWithPattern(
      base::Time::Now(), "y-MM-dd - HH.mm.ss"));
}

DebugOverlayHandler::~DebugOverlayHandler() = default;

void DebugOverlayHandler::DeclareJSCallbacks() {
  AddCallback("debug.captureScreenshot",
              &DebugOverlayHandler::HandleCaptureScreenshot);
  AddCallback("debug.toggleColorMode", &DebugOverlayHandler::ToggleColorMode);
  AddCallback("debug.switchWallpaper",
              &DebugOverlayHandler::HandleSwitchWallpaper);
}

void DebugOverlayHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

// DebugOverlayHandler, private: ----------------------------------------------

void DebugOverlayHandler::HandleCaptureScreenshot(const std::string& name) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  if (root_windows.size() == 0)
    return;

  screenshot_index_++;
  std::string filename_base =
      base::StringPrintf("%04d - %s", screenshot_index_, name.c_str());

  for (size_t screen = 0; screen < root_windows.size(); ++screen) {
    aura::Window* root_window = root_windows[screen];
    gfx::Rect rect = root_window->bounds();
    std::string filename = filename_base;
    if (root_windows.size() > 1) {
      filename.append(base::StringPrintf("- Display %zu", screen));
    }

    if (add_resolution_to_filename_)
      filename.append("_" + rect.size().ToString());

    if (DarkLightModeController::Get()->IsDarkModeEnabled()) {
      filename.append("_dark");
    }

    filename.append(".png");
    ui::GrabWindowSnapshotAsPNG(root_window, rect,
                                base::BindOnce(&RunStoreScreenshotOnTaskRunner,
                                               screenshot_dir_, filename));
  }
}

void DebugOverlayHandler::ToggleColorMode() {
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(  // IN-TEST
      !DarkLightModeController::Get()->IsDarkModeEnabled());
}

void DebugOverlayHandler::HandleSwitchWallpaper(const std::string& color) {
  if (color == "def") {
    ash::WallpaperController::Get()->ShowOobeWallpaper();
    return;
  }

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  if (color == "wh") {
    bitmap.eraseColor(SK_ColorWHITE);
  } else if (color == "bk") {
    bitmap.eraseColor(SK_ColorBLACK);
  } else if (color == "r") {
    bitmap.eraseColor(SK_ColorRED);
  } else if (color == "bl") {
    bitmap.eraseColor(SK_ColorBLUE);
  } else if (color == "gn") {
    bitmap.eraseColor(SK_ColorGREEN);
  } else if (color == "ye") {
    bitmap.eraseColor(SK_ColorYELLOW);
  } else {
    return;
  }
  ash::WallpaperController::Get()->ShowOneShotWallpaper(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

}  // namespace ash
