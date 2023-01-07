// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_LACROS_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_SHELF_LACROS_APP_WINDOW_H_

#include "chrome/browser/ui/ash/shelf/app_window_base.h"

namespace ash {
struct ShelfID;
}

namespace views {
class Widget;
}

// An AppWindowBase (and hence ui::BaseWindow) for a Lacros browser. Handles
// setting the window icon and app icon.
class LacrosAppWindow : public AppWindowBase {
 public:
  LacrosAppWindow(const ash::ShelfID& shelf_id, views::Widget* widget);
  LacrosAppWindow(const LacrosAppWindow&) = delete;
  LacrosAppWindow& operator=(const LacrosAppWindow&) = delete;
  ~LacrosAppWindow() override;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_LACROS_APP_WINDOW_H_
