// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"

class AppServiceAppIconLoader;
class ArcAppWindowDelegate;

namespace gfx {
class ImageSkia;
}

namespace views {
class Widget;
}

class Profile;

// A ui::BaseWindow for a chromeos launcher to control ARC applications.
class ArcAppWindow : public AppWindowBase,
                     public AppIconLoaderDelegate {
 public:
  ArcAppWindow(const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowDelegate* owner,
               Profile* profile);

  ArcAppWindow(const ArcAppWindow&) = delete;
  ArcAppWindow& operator=(const ArcAppWindow&) = delete;

  ~ArcAppWindow() override;

  void SetFullscreenMode(FullScreenMode mode) override;

  // Sets optional window title and icon.
  void SetDescription(const std::string& title,
                      const gfx::ImageSkia& icon) override;

  FullScreenMode fullscreen_mode() const { return fullscreen_mode_; }

  const arc::ArcAppShelfId& app_shelf_id() const { return app_shelf_id_; }

  // ui::BaseWindow:
  bool IsActive() const override;
  void Close() override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

 private:
  // Ensures that default app icon is set.
  void SetDefaultAppIcon();

  // Sets the icon for the window.
  void SetIcon(const gfx::ImageSkia& icon);

  // Keeps ARC shelf grouping id.
  const arc::ArcAppShelfId app_shelf_id_;
  // Keeps current full-screen mode.
  FullScreenMode fullscreen_mode_ = FullScreenMode::kNotDefined;
  const raw_ptr<ArcAppWindowDelegate> owner_;

  // Set to true in case image fetch is requested. This indicates that default
  // app icon is returned in |OnAppImageUpdated|.
  bool image_fetching_ = false;
  base::OneShotTimer apply_default_image_timer_;

  const raw_ptr<Profile> profile_;

  // Loads the ARC app icon to the window icon keys. Nullptr once a custom icon
  // has been successfully set.
  std::unique_ptr<AppServiceAppIconLoader> app_icon_loader_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_H_
