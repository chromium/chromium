// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_BASE_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_BASE_H_

#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/base/base_window.h"

class AppWindowLauncherItemController;

namespace views {
class Widget;
}

// A ui::BaseWindow for a Chrome OS shelf to control CrOS apps, e.g. ARC++,
// Crostini, and interal apps.
class AppWindowBase : public ui::BaseWindow {
 public:
  AppWindowBase(const ash::ShelfID& shelf_id, views::Widget* widget);

  virtual ~AppWindowBase() {}

  void SetController(AppWindowLauncherItemController* controller);

  const std::string& app_id() const { return shelf_id_.app_id; }

  const ash::ShelfID& shelf_id() const { return shelf_id_; }

  void set_shelf_id(const ash::ShelfID& shelf_id) { shelf_id_ = shelf_id; }

  views::Widget* widget() const { return widget_; }

  AppWindowLauncherItemController* controller() const { return controller_; }

  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

 private:
  ash::ShelfID shelf_id_;
  views::Widget* const widget_;
  AppWindowLauncherItemController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppWindowBase);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_BASE_H_
