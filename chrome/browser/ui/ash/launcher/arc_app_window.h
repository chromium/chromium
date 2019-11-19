// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"

class ArcAppWindowLauncherController;

namespace gfx {
class ImageSkia;
}

namespace views {
class Widget;
}

class Profile;

// A ui::BaseWindow for a chromeos launcher to control ARC applications.
class ArcAppWindow : public AppWindowBase,
                     public ImageDecoder::ImageRequest,
                     public AppIconLoaderDelegate {
 public:
  // TODO(khmel): use a bool set to false by default, or use an existing enum,
  // like ash::WindowStateType.
  enum class FullScreenMode {
    NOT_DEFINED,  // Fullscreen mode was not defined.
    ACTIVE,       // Fullscreen is activated for an app.
    NON_ACTIVE,   // Fullscreen was not activated for an app.
  };

  ArcAppWindow(int task_id,
               const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowLauncherController* owner,
               Profile* profile);

  ~ArcAppWindow() override;

  void SetFullscreenMode(FullScreenMode mode);

  // Sets optional window title and icon. Note that |unsafe_icon_data_png| has
  // to be decoded in separate process for security reason.
  void SetDescription(const std::string& title,
                      const std::vector<uint8_t>& unsafe_icon_data_png);

  FullScreenMode fullscreen_mode() const { return fullscreen_mode_; }

  int task_id() const { return task_id_; }

  const arc::ArcAppShelfId& app_shelf_id() const { return app_shelf_id_; }

  // ui::BaseWindow:
  bool IsActive() const override;
  void Close() override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

 private:
  // Ensures that default app icon is set.
  void SetDefaultAppIcon();

  // Sets the icon for the window.
  void SetIcon(const gfx::ImageSkia& icon);

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;

  // Keeps associated ARC task id.
  const int task_id_;
  // Keeps ARC shelf grouping id.
  const arc::ArcAppShelfId app_shelf_id_;
  // Keeps current full-screen mode.
  FullScreenMode fullscreen_mode_ = FullScreenMode::NOT_DEFINED;
  ArcAppWindowLauncherController* const owner_;

  // Set to true in case image fetch is requested. This indicates that default
  // app icon is returned in |OnAppImageUpdated|.
  bool image_fetching_ = false;
  base::OneShotTimer apply_default_image_timer_;

  Profile* const profile_;

  // Loads the ARC app icon to the window icon keys. Nullptr once a custom icon
  // has been successfully set.
  std::unique_ptr<ArcAppIconLoader> app_icon_loader_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindow);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_
