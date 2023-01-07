// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_WINDOW_H_

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"

namespace views {
class Widget;
}

class Profile;

// A ui::BaseWindow for a chromeos launcher to control Crostini applications.
// Also handles loading the window icon and app icon for the application.
class CrostiniAppWindow : public AppWindowBase {
 public:
  CrostiniAppWindow(Profile* profile,
                    const ash::ShelfID& shelf_id,
                    views::Widget* widget);

  ~CrostiniAppWindow() override;

  CrostiniAppWindow(const CrostiniAppWindow&) = delete;
  CrostiniAppWindow& operator=(const CrostiniAppWindow&) = delete;

 private:
  class IconLoader;

  // Loads the app icon to the window's app icon key. The app icon is larger
  // than the window icon, and is used for things like Alt-Tab.
  std::unique_ptr<IconLoader> app_icon_loader_;

  // Loads the window icon to the window icon key. The window icon is smaller
  // than the app icon, and is used for things like shelf app menus.
  std::unique_ptr<IconLoader> window_icon_loader_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_WINDOW_H_
