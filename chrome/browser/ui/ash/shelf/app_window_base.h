// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_BASE_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_BASE_H_

#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

class AppWindowShelfItemController;

namespace gfx {
class ImageSkia;
}

namespace views {
class Widget;
}

// A ui::BaseWindow for a Chrome OS shelf to control CrOS apps, e.g. ARC++,
// Crostini, and interal apps.
class AppWindowBase : public ui::BaseWindow {
 public:
  enum class FullScreenMode {
    kNotDefined,  // Fullscreen mode was not defined.
    kActive,      // Fullscreen is activated for an app.
    kNonActive,   // Fullscreen was not activated for an app.
  };

  AppWindowBase(const ash::ShelfID& shelf_id, views::Widget* widget);

  AppWindowBase(const AppWindowBase&) = delete;
  AppWindowBase& operator=(const AppWindowBase&) = delete;

  virtual ~AppWindowBase();

  void SetController(AppWindowShelfItemController* controller);

  const std::string& app_id() const { return shelf_id_.app_id; }

  const ash::ShelfID& shelf_id() const { return shelf_id_; }

  void set_shelf_id(const ash::ShelfID& shelf_id) { shelf_id_ = shelf_id; }

  views::Widget* widget() const { return widget_; }

  AppWindowShelfItemController* controller() const { return controller_; }

  virtual void SetDescription(const std::string& title,
                              const gfx::ImageSkia& icon) {}

  virtual void SetFullscreenMode(FullScreenMode mode) {}

  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
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
  const raw_ptr<views::Widget> widget_;
  raw_ptr<AppWindowShelfItemController> controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_BASE_H_
