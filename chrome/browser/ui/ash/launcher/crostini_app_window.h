// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class Widget;
}

class AppServiceAppIconLoader;
class Profile;

// A ui::BaseWindow for a chromeos launcher to control Crostini applications.
class CrostiniAppWindow : public AppWindowBase, public AppIconLoaderDelegate {
 public:
  CrostiniAppWindow(Profile* profile,
                    const ash::ShelfID& shelf_id,
                    views::Widget* widget);

  ~CrostiniAppWindow() override;

  CrostiniAppWindow(const CrostiniAppWindow&) = delete;
  CrostiniAppWindow& operator=(const CrostiniAppWindow&) = delete;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

 private:
  // Loads the app icon to the window icon key.
  std::unique_ptr<AppServiceAppIconLoader> app_icon_loader_;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_H_
