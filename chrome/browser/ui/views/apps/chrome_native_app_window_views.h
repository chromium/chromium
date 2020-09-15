// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "extensions/components/native_app_window/native_app_window_views.h"

class ExtensionKeybindingRegistryViews;

class ChromeNativeAppWindowViews
    : public native_app_window::NativeAppWindowViews,
      public extensions::ChromeAppIconDelegate {
 public:
  ChromeNativeAppWindowViews();
  ~ChromeNativeAppWindowViews() override;

  SkRegion* shape() { return shape_.get(); }
  ShapeRects* shape_rects() { return shape_rects_.get(); }

 protected:
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget);
  virtual void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame();
  virtual views::NonClientFrameView* CreateNonStandardAppFrame() = 0;
  virtual bool ShouldRemoveStandardFrame();

  // ui::BaseWindow implementation.
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  ui::ZOrderLevel GetZOrderLevel() const override;

  // WidgetDelegate implementation.
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;

  // views::View implementation.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // NativeAppWindow implementation.
  void SetFullscreen(int fullscreen_types) override;
  bool IsFullscreenOrPending() const override;
  void UpdateShape(std::unique_ptr<ShapeRects> rects) override;
  bool HasFrameColor() const override;
  SkColor ActiveFrameColor() const override;
  SkColor InactiveFrameColor() const override;

  // NativeAppWindowViews implementation.
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

 private:
  // Ensures that the Chrome app icon is created.
  void EnsureAppIconCreated();

  // extensions::ChromeAppIconDelegate:
  void OnIconUpdated(extensions::ChromeAppIcon* icon) override;

  // Custom shape of the window. If this is not set then the window has a
  // default shape, usually rectangular.
  std::unique_ptr<SkRegion> shape_;

  std::unique_ptr<ShapeRects> shape_rects_;

  bool has_frame_color_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  // Contains the default Chrome app icon. It is used in case the custom icon
  // for the extension app window is not set, or as a part of composite image.
  std::unique_ptr<extensions::ChromeAppIcon> app_icon_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
